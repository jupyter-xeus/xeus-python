/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and     *
* Wolf Vollprecht                                                          *
* Copyright (c) 2018, QuantStack                                           *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

#include "xeus/xinterpreter.hpp"

#include "pybind11_json/pybind11_json.hpp"

#include "pybind11/pybind11.h"
#include "pybind11/functional.h"
#include "pybind11/stl.h"

#include "xeus-python/xutils.hpp"

#include "xdisplay.hpp"
#include "xinternal_utils.hpp"

#ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wattributes"
#endif

namespace py = pybind11;
namespace nl = nlohmann;
using namespace pybind11::literals;

namespace xpyt_ipython
{
    /****************************************
     * xpublish_display_data implementation *
     ****************************************/

    void xpublish_display_data(const py::object& data, const py::object& metadata, const py::object& transient, bool update)
    {
        auto& interp = xeus::get_interpreter();

        // Make sure transient is not None
        py::object transient_ = transient;
        if (transient_.is_none())
        {
            transient_ = py::dict();
        }

        if (update)
        {
            interp.update_display_data(data, metadata, transient_);
        }
        else
        {
            interp.display_data(data, metadata, transient_);
        }
    }

    /********************************************
     * xpublish_execution_result implementation *
     ********************************************/

    void xpublish_execution_result(const py::int_& execution_count, const py::object& data, const py::object& metadata)
    {
        auto& interp = xeus::get_interpreter();

        nl::json cpp_data = data;
        if (cpp_data.size() != 0)
        {
            interp.publish_execution_result(execution_count, std::move(cpp_data), metadata);
        }
    }

    /*************************
     * xclear implementation *
     *************************/

    void xclear(bool wait = false)
    {
        auto& interp = xeus::get_interpreter();

        interp.clear_output(wait);
    }

    /******************
     * display module *
     ******************/

    py::module get_display_module_impl()
    {
        py::module display_module = xpyt::create_module("display");

        display_module.def("publish_display_data",
            xpublish_display_data,
            "data"_a,
            "metadata"_a=py::dict(),
            "transient"_a=py::dict(),
            "update"_a=false
        );

        display_module.def("publish_execution_result",
            xpublish_execution_result,
            "execution_count"_a,
            "data"_a,
            "metadata"_a=py::dict()
        );

        display_module.def("clear_output",
            xclear,
            "wait"_a=false
        );

        return display_module;
    }
}

namespace xpyt_raw
{

    bool safe_exists(const py::object& path)
    {
        py::module os = py::module::import("os");

        try
        {
            return xpyt::is_pyobject_true(os.attr("path").attr("exists")(path));
        }
        catch (py::error_already_set& e)
        {
            return false;
        }
    }

    bool should_include(const std::string& mimetype, const std::vector<std::string>& include)
    {
        return include.size() == 0 || std::find(include.cbegin(), include.cend(), mimetype) != include.end();
    }

    bool should_exclude(const std::string& mimetype, const std::vector<std::string>& exclude)
    {
        return exclude.size() != 0 && std::find(exclude.cbegin(), exclude.cend(), mimetype) != exclude.end();
    }

    void compute_repr(
        const py::object& obj, const std::string& repr_method, const std::string& mimetype,
        const std::vector<std::string>& include, const std::vector<std::string>& exclude,
        py::dict& pub_data, py::dict& pub_metadata)
    {
        if (hasattr(obj, repr_method.c_str()) && should_include(mimetype, include) && !should_exclude(mimetype, exclude))
        {
            const py::object& repr = obj.attr(repr_method.c_str())();

            if (!repr.is_none())
            {
                if (py::isinstance<py::tuple>(repr))
                {
                    py::tuple repr_tuple = repr;

                    pub_data[mimetype.c_str()] = repr_tuple[0];
                    pub_metadata[mimetype.c_str()] = repr_tuple[1];
                }
                else
                {
                    pub_data[mimetype.c_str()] = repr;
                }
            }
        }
    }

    py::tuple mime_bundle_repr(const py::object& obj, const std::vector<std::string>& include = {}, const std::vector<std::string>& exclude = {})
    {
        py::module py_json = py::module::import("json");
        py::module builtins = py::module::import("builtins");
        py::dict pub_data;
        py::dict pub_metadata;

        if (hasattr(obj, "_repr_mimebundle_"))
        {
            pub_data = obj.attr("_repr_mimebundle_")(include, exclude);
        }
        else
        {
            compute_repr(obj, "_repr_html_", "text/html", include, exclude, pub_data, pub_metadata);
            compute_repr(obj, "_repr_markdown_", "text/markdown", include, exclude, pub_data, pub_metadata);
            compute_repr(obj, "_repr_svg_", "image/svg+xml", include, exclude, pub_data, pub_metadata);
            compute_repr(obj, "_repr_png_", "image/png", include, exclude, pub_data, pub_metadata);
            compute_repr(obj, "_repr_jpeg_", "image/jpeg", include, exclude, pub_data, pub_metadata);
            compute_repr(obj, "_repr_latex_", "text/latex", include, exclude, pub_data, pub_metadata);
            compute_repr(obj, "_repr_json_", "application/json", include, exclude, pub_data, pub_metadata);
            compute_repr(obj, "_repr_javascript_", "application/javascript", include, exclude, pub_data, pub_metadata);
            compute_repr(obj, "_repr_pdf_", "application/pdf", include, exclude, pub_data, pub_metadata);
        }

        pub_data["text/plain"] = py::str(builtins.attr("repr")(obj));

        return py::make_tuple(pub_data, pub_metadata);
    }

    /****************************
     * xdisplayhook declaration *
     ****************************/

    class xdisplayhook
    {
    public:

        xdisplayhook();
        virtual ~xdisplayhook();

        void set_execution_count(int execution_count);
        void operator()(const py::object& obj, bool raw) const;

    private:

        int m_execution_count;
    };

    /*******************************
     * xdisplayhook implementation *
     *******************************/

    xdisplayhook::xdisplayhook()
        : m_execution_count(0)
    {
    }

    xdisplayhook::~xdisplayhook()
    {
    }

    void xdisplayhook::set_execution_count(int execution_count)
    {
        m_execution_count = execution_count;
    }

    void xdisplayhook::operator()(const py::object& obj, bool raw = false) const
    {
        auto& interp = xeus::get_interpreter();

        if (!obj.is_none())
        {
            if (hasattr(obj, "_ipython_display_"))
            {
                obj.attr("_ipython_display_")();
                return;
            }

            py::object pub_data;
            py::object pub_metadata;
            if (raw)
            {
                pub_data = obj;
            }
            else
            {
                const py::tuple& repr = mime_bundle_repr(obj);
                pub_data = repr[0];
                pub_metadata = repr[1];
            }

            interp.publish_execution_result(m_execution_count, pub_data, pub_metadata);
        }
    }

    /***************************
     * xdisplay implementation *
     ***************************/

    void xdisplay_impl(const py::args objs,
        const std::vector<std::string>& include,
        const std::vector<std::string>& exclude,
        const py::dict& metadata,
        const py::object& transient,
        const py::object& display_id,
        bool update,
        bool raw)
    {
        auto& interp = xeus::get_interpreter();

        for (std::size_t i = 0; i < objs.size(); ++i)
        {
            py::object obj = objs[i];
            if (!obj.is_none())
            {
                if (hasattr(obj, "_ipython_display_"))
                {
                    obj.attr("_ipython_display_")();
                    return;
                }

                py::object pub_data;
                py::object pub_metadata = py::dict();
                if (raw)
                {
                    pub_data = obj;
                }
                else
                {
                    const py::tuple& repr = mime_bundle_repr(obj, include, exclude);
                    pub_data = repr[0];
                    pub_metadata = repr[1];
                }
                pub_metadata.attr("update")(metadata);

                nl::json cpp_transient = transient.is_none() ? nl::json::object() : nl::json(transient);

                if (!display_id.is_none())
                {
                    cpp_transient["display_id"] = display_id;
                }
                if (update)
                {
                    interp.update_display_data(pub_data, pub_metadata, std::move(cpp_transient));
                }
                else
                {
                    interp.display_data(pub_data, pub_metadata, std::move(cpp_transient));
                }
            }
        }
    }

    void xdisplay(py::args objs, py::kwargs kw)
    {
        auto raw = kw.contains("raw") ? kw["raw"].cast<bool>() : false;
        auto include = kw.contains("include") ? kw["include"].cast<std::vector<std::string>>() : std::vector<std::string>();
        auto exclude = kw.contains("exclude") ? kw["exclude"].cast<std::vector<std::string>>() : std::vector<std::string>({});
        auto metadata = kw.contains("metadata") ? py::object(kw["metadata"]) : py::dict();
        auto transient = kw.contains("transient") ? py::object(kw["transient"]) : py::none();
        auto display_id = kw.contains("display_id") ? py::object(kw["display_id"]) : py::none();

        xdisplay_impl(objs, include, exclude, metadata, transient, display_id, false, raw);
    }

    void xupdate_display(py::args objs, py::kwargs kw)
    {
        auto raw = kw.contains("raw") ? kw["raw"].cast<bool>() : false;
        auto include = kw.contains("include") ? kw["include"].cast<std::vector<std::string>>() : std::vector<std::string>();
        auto exclude = kw.contains("exclude") ? kw["exclude"].cast<std::vector<std::string>>() : std::vector<std::string>({});
        auto metadata = kw.contains("metadata") ? py::object(kw["metadata"]) : py::dict();
        auto transient = kw.contains("transient") ? py::object(kw["transient"]) : py::none();
        auto display_id = kw.contains("display_id") ? py::object(kw["display_id"]) : py::none();

        xdisplay_impl(objs, include, exclude, metadata, transient, display_id, true, raw);
    }

    void xpublish_display_data(const py::object& data, const py::object& metadata, const py::str& /*source*/, const py::object& transient)
    {
        auto& interp = xeus::get_interpreter();

        interp.display_data(data, metadata, transient);
    }

    void xdisplay_mimetype(const std::string& mimetype, py::args objs, py::kwargs kw)
    {
        bool raw = kw.contains("raw") ? kw["raw"].cast<bool>() : false;
        py::object metadata = kw.contains("metadata") ? kw["metadata"] : py::dict();
        py::object p_metadata = py::dict();

        if (!metadata.is_none())
        {
            p_metadata = py::dict(py::arg(mimetype.c_str()) = metadata);
        }

        py::list disp_objs = objs;
        if (raw)
        {
            for (std::size_t i = 0; i < objs.size(); ++i)
            {
                disp_objs[i] = py::dict(py::arg(mimetype.c_str()) = objs[i]);
            }
        }
        xdisplay_impl(disp_objs, { mimetype }, {}, p_metadata, py::none(), py::none(), false, raw);
    }

    void xdisplay_html(py::args objs, py::kwargs kw)
    {
        xdisplay_mimetype("text/html", objs, kw);
    }

    void xdisplay_markdown(py::args objs, py::kwargs kw)
    {
        xdisplay_mimetype("text/markdown", objs, kw);
    }

    void xdisplay_svg(py::args objs, py::kwargs kw)
    {
        xdisplay_mimetype("image/svg+xml", objs, kw);
    }

    void xdisplay_png(py::args objs, py::kwargs kw)
    {
        xdisplay_mimetype("image/png", objs, kw);
    }

    void xdisplay_jpeg(py::args objs, py::kwargs kw)
    {
        xdisplay_mimetype("image/jpeg", objs, kw);
    }

    void xdisplay_latex(py::args objs, py::kwargs kw)
    {
        xdisplay_mimetype("text/latex", objs, kw);
    }

    void xdisplay_json(py::args objs, py::kwargs kw)
    {
        xdisplay_mimetype("application/json", objs, kw);
    }

    void xdisplay_javascript(py::args objs, py::kwargs kw)
    {
        xdisplay_mimetype("application/javascript", objs, kw);
    }

    void xdisplay_pdf(py::args objs, py::kwargs kw)
    {
        xdisplay_mimetype("application/pdf", objs, kw);
    }

    /*************************
     * xclear implementation *
     *************************/

    void xclear(bool wait = false)
    {
        auto& interp = xeus::get_interpreter();
        interp.clear_output(wait);
    }

    /*******************************
     * xdisplay_object declaration *
     *******************************/

    class xdisplay_object
    {
    public:

        xdisplay_object(const py::object& data, const py::object& url, const py::object& filename, const py::object& metadata = py::dict(), const std::string& read_flag = "r");
        virtual ~xdisplay_object();

        xdisplay_object(const xdisplay_object&) = delete;
        xdisplay_object(xdisplay_object&&) = delete;

        xdisplay_object& operator=(const xdisplay_object&) = delete;
        xdisplay_object& operator=(xdisplay_object&&) = delete;

        void reload();

        py::object get_metadata();
        virtual void set_metadata(const py::object& data);
        py::object get_data();
        virtual void set_data(const py::object& data);

    protected:

        py::object data_and_metadata() const;

    private:

        py::object m_data;
        py::object m_url = py::none();
        py::object m_filename = py::none();
        py::object m_metadata = py::none();
        py::str m_read_flag;
    };

    /**********************************
     * xdisplay_object implementation *
     **********************************/

    xdisplay_object::xdisplay_object(const py::object& data, const py::object& url, const py::object& filename, const py::object& metadata, const std::string& read_flag)
        : m_data(data), m_url(url), m_filename(filename), m_metadata(metadata), m_read_flag(read_flag)
    {
        py::module pathlib = py::module::import("pathlib");

        if (py::isinstance(data, py::make_tuple(pathlib.attr("Path"), pathlib.attr("PurePath"))))
        {
            m_data = py::str(data);
        }

        if (!m_data.is_none() && py::isinstance<py::str>(m_data))
        {
            if (xpyt::is_pyobject_true(m_data.attr("startswith")("http")) && url.is_none())
            {
                m_url = m_data;
                m_filename = py::none();
                m_data = py::none();
            }
            else if (safe_exists(m_data) && filename.is_none())
            {
                m_url = py::none();
                m_filename = m_data;
                m_data = py::none();
            }
        }

        reload();
    }

    xdisplay_object::~xdisplay_object()
    {
    }

    py::object xdisplay_object::data_and_metadata() const
    {
        py::module copy = py::module::import("copy");

        if (m_metadata.is_none())
        {
            return m_data;
        }
        else
        {
            return py::make_tuple(m_data, copy.attr("deepcopy")(m_metadata));
        }
    }

    py::object xdisplay_object::get_metadata()
    {
        return m_metadata;
    }

    void xdisplay_object::set_metadata(const py::object& data)
    {
        m_metadata = data;
    }

    py::object xdisplay_object::get_data()
    {
        return m_data;
    }

    void xdisplay_object::set_data(const py::object& data)
    {
        m_data = data;
    }

    void xdisplay_object::reload()
    {
        py::module builtins = py::module::import("builtins");

        if (!m_filename.is_none())
        {
            py::object fobj;
            try
            {
                fobj = builtins.attr("open")(m_filename, m_read_flag);
                set_data(fobj.attr("read")());
            }
            catch (py::error_already_set& e)
            {
                fobj.attr("close")();

                throw e;
            }
        }
        else if (!m_url.is_none())
        {
            try
            {
                py::module request = py::module::import("urllib.request");
                py::object response = request.attr("urlopen")(m_url);

                py::object content = response.attr("read")();

                py::object encoding = py::none();
                for (py::handle sub : response.attr("headers")["content-type"].attr("split")(";"))
                {
                    sub = sub.attr("strip")();
                    if (xpyt::is_pyobject_true(sub.attr("startswith")("charset")))
                    {
                        py::list splitted = sub.attr("split")("=");
                        encoding = splitted[py::len(splitted) - 1].attr("strip")();
                        break;
                    }
                }

                if (!encoding.is_none())
                {
                    set_data(content.attr("decode")(encoding, "replace"));
                }
                else
                {
                    set_data(content);
                }
            }
            catch (py::error_already_set& e)
            {
                set_data(py::none());
            }
        }
    }

    /******************************
     * xtext_display_object class *
     ******************************/

    class xtext_display_object : public xdisplay_object
    {
    public:

        xtext_display_object(const py::object& data, const py::object& url, const py::object& filename, const py::object& metadata = py::dict());
        virtual ~xtext_display_object();

    };

    xtext_display_object::xtext_display_object(const py::object& data, const py::object& url, const py::object& filename, const py::object& metadata)
        : xdisplay_object(data, url, filename, metadata)
    {
    }

    xtext_display_object::~xtext_display_object()
    {
    }

    /***************
     * xhtml class *
     ***************/

    class xhtml : public xdisplay_object
    {
    public:

        xhtml(const py::object& data, const py::object& url, const py::object& filename, const py::object& metadata);
        virtual ~xhtml();

        py::object repr_html() const;
        py::object html() const;

    };

    xhtml::xhtml(const py::object& data, const py::object& url, const py::object& filename, const py::object& metadata)
        : xdisplay_object(data, url, filename, metadata)
    {
    }

    xhtml::~xhtml()
    {
    }

    py::object xhtml::repr_html() const
    {
        return data_and_metadata();
    }

    py::object xhtml::html() const
    {
        return repr_html();
    }

    /*********************
     * xjavascript class *
     *********************/

    class xjavascript : public xtext_display_object
    {
    public:

        xjavascript(const py::object& data, const py::object& url, const py::object& filename, const py::object& lib, const py::object& css);
        virtual ~xjavascript();

        py::object repr_javascript();

    private:

        py::list m_lib;
        py::list m_css;
    };

    xjavascript::xjavascript(const py::object& data, const py::object& url, const py::object& filename, const py::object& lib, const py::object& css)
        : xtext_display_object(data, url, filename)
    {
        if (py::isinstance<py::str>(lib))
        {
            m_lib = py::list({ lib });
        }
        else if (lib.is_none())
        {
            m_lib = py::list();
        }
        else
        {
            m_lib = lib;
        }

        if (py::isinstance<py::str>(css))
        {
            m_css = py::list({ css });
        }
        else if (css.is_none())
        {
            m_css = py::list();
        }
        else
        {
            m_css = css;
        }
    }

    xjavascript::~xjavascript()
    {
    }

    py::object xjavascript::repr_javascript()
    {
        std::ostringstream string_stream;

        for (py::handle css : m_css)
        {
            string_stream << R"($("head").append($("<link/>").attr({rel:  "stylesheet", type: "text/css", href: ")";
            string_stream << css.cast<std::string>();
            string_stream << R"("})))";
        }

        for (py::handle lib : m_lib)
        {
            string_stream << R"($.getScript(")";
            string_stream << lib.cast<std::string>();
            string_stream << R"(", function () {)";
        }

        string_stream << get_data().cast<std::string>();

        for (std::size_t i = 0; i < py::len(m_lib); i++)
        {
            string_stream << "});";
        }

        return py::str(string_stream.str());
    }

    /*******************
     * xmarkdown class *
     *******************/

    class xmarkdown : public xdisplay_object
    {
    public:

        xmarkdown(const py::object& data, const py::object& url, const py::object& filename, const py::object& metadata);
        virtual ~xmarkdown();

        py::object repr_markdown() const;

    };

    xmarkdown::xmarkdown(const py::object& data, const py::object& url, const py::object& filename, const py::object& metadata)
        : xdisplay_object(data, url, filename, metadata)
    {
    }

    xmarkdown::~xmarkdown()
    {
    }

    py::object xmarkdown::repr_markdown() const
    {
        return data_and_metadata();
    }

    /***************
     * xmath class *
     ***************/

    class xmath : public xdisplay_object
    {
    public:

        xmath(const py::object& data, const py::object& url, const py::object& filename, const py::object& metadata);
        virtual ~xmath();

        py::object repr_latex();

    };

    xmath::xmath(const py::object& data, const py::object& url, const py::object& filename, const py::object& metadata)
        : xdisplay_object(data, url, filename, metadata)
    {
    }

    xmath::~xmath()
    {
    }

    py::object xmath::repr_latex()
    {
        py::module copy = py::module::import("copy");

        std::ostringstream string_stream;
        string_stream << R"($\displaystyle )" << get_data().attr("strip")("$").cast<std::string>() << "$";
        py::str s = py::str(string_stream.str());

        if (get_metadata().is_none())
        {
            return s;
        }
        else
        {
            return py::make_tuple(s, copy.attr("deepcopy")(get_metadata()));
        }
    }

    /****************
     * xlatex class *
     ****************/

    class xlatex : public xdisplay_object
    {
    public:

        xlatex(const py::object& data, const py::object& url, const py::object& filename, const py::object& metadata);
        virtual ~xlatex();

        py::object repr_latex() const;

    };

    xlatex::xlatex(const py::object& data, const py::object& url, const py::object& filename, const py::object& metadata)
        : xdisplay_object(data, url, filename, metadata)
    {
    }

    xlatex::~xlatex()
    {
    }

    py::object xlatex::repr_latex() const
    {
        return data_and_metadata();
    }

    /**************
     * xsvg class *
     **************/

    class xsvg : public xdisplay_object
    {
    public:

        xsvg(const py::object& data, const py::object& url, const py::object& filename, const py::object& metadata);
        virtual ~xsvg();

        py::object repr_svg() const;

    protected:

        void set_data(const py::object& data) override;

    };

    xsvg::xsvg(const py::object& data, const py::object& url, const py::object& filename, const py::object& metadata)
        : xdisplay_object(data, url, filename, metadata, "rb")
    {
    }

    xsvg::~xsvg()
    {
    }

    void xsvg::set_data(const py::object& data)
    {
        if (data.is_none())
        {
            xdisplay_object::set_data(data);
            return;
        }

        py::object svg = data;

        py::module minidom = py::module::import("xml.dom.minidom");
        py::list found_svg = minidom.attr("parseString")(data).attr("getElementsByTagName")("svg");

        if (py::len(found_svg) != 0)
        {
            svg = found_svg[0].attr("toxml")();
        }

        xdisplay_object::set_data(svg);
    }

    py::object xsvg::repr_svg() const
    {
        return data_and_metadata();
    }

    /***************
     * xjson class *
     ***************/

    class xjson : public xdisplay_object
    {
    public:

        xjson(
            const py::object& data, const py::object& url, const py::object& filename,
            const py::bool_& expanded, const py::object& metadata, const py::str& root
        );
        virtual ~xjson();

        py::object repr_json() const;

    protected:

        void set_data(const py::object& data) override;

    };

    xjson::xjson(
        const py::object& data, const py::object& url, const py::object& filename,
        const py::bool_& expanded, const py::object& metadata, const py::str& root)
        : xdisplay_object(data, url, filename, metadata)
    {
        if (get_metadata().is_none())
        {
            set_metadata(py::dict("expanded"_a = expanded, "root"_a = root));
        }
        else
        {
            get_metadata().attr("update")(py::dict("expanded"_a = expanded, "root"_a = root));
        }
    }

    xjson::~xjson()
    {
    }

    void xjson::set_data(const py::object& data)
    {
        py::module pathlib = py::module::import("pathlib");

        if (py::isinstance(data, py::make_tuple(pathlib.attr("Path"), pathlib.attr("PurePath"))))
        {
            xdisplay_object::set_data(py::str(data));
            return;
        }

        if (py::isinstance<py::str>(data))
        {
            py::module json = py::module::import("json");

            xdisplay_object::set_data(json.attr("loads")(data));
            return;
        }

        xdisplay_object::set_data(data);
    }

    py::object xjson::repr_json() const
    {
        return data_and_metadata();
    }

    /******************
     * xgeojson class *
     ******************/

    class xgeojson : public xjson
    {
    public:

        xgeojson(
            const py::object& data, const py::object& url, const py::object& filename,
            const py::bool_& expanded, const py::object& metadata, const py::str& root,
            const py::dict& layer_options, const py::str& url_template
        );
        virtual ~xgeojson();

        void ipython_display();

    private:

        py::dict m_layer_options;
        py::str m_url_template;
    };

    xgeojson::xgeojson(
        const py::object& data, const py::object& url, const py::object& filename,
        const py::bool_& expanded, const py::object& metadata, const py::str& root,
        const py::dict& layer_options, const py::str& url_template)
        : xjson(data, url, filename, expanded, metadata, root), m_layer_options(layer_options), m_url_template(url_template)
    {
        const py::dict& p_metadata = get_metadata();
        p_metadata["layer_options"] = m_layer_options;
        p_metadata["url_template"] = m_url_template;
    }

    xgeojson::~xgeojson()
    {
    }

    void xgeojson::ipython_display()
    {
        py::dict bundle = py::dict("application/geo+json"_a = get_data(), "text/plain"_a = "<IPython.display.GeoJSON object>");
        py::dict metadata = py::dict("application/geo+json"_a = get_metadata());

        xdisplay(bundle, py::kwargs());
    }

    /**********************
     * xprogressbar class *
     **********************/

    class xprogressbar
    {
    public:

        xprogressbar(std::ptrdiff_t total);

        std::string repr() const;
        std::string repr_html() const;
        const xprogressbar& iter();
        std::ptrdiff_t next();

        std::ptrdiff_t get_progress() const;
        void set_progress(std::ptrdiff_t);

        std::ptrdiff_t get_total() const;
        void set_total(std::ptrdiff_t);

    private:

        void display(bool update) const;

        std::ptrdiff_t m_progress = 0;
        std::ptrdiff_t m_total;

        std::size_t m_text_width = 60;

        xeus::xguid m_id;

    };

    xprogressbar::xprogressbar(std::ptrdiff_t total)
        : m_total(total), m_id(xeus::new_xguid())
    {
    }

    std::string xprogressbar::repr() const
    {
        double fraction = double(m_progress) / double(m_total);
        std::size_t len_filled = floor(fraction * m_text_width);

        std::string filled(len_filled, '=');
        std::string rest(m_text_width - len_filled, ' ');

        std::ostringstream string_stream;
        string_stream << "[" << filled << rest << "] " << m_progress << "/" << m_total;

        return string_stream.str();
    }

    std::string xprogressbar::repr_html() const
    {
        std::ostringstream string_stream;
        string_stream << "<progress style='width:60ex' max='" << m_total << "' value='" << m_progress << "'></progress>";

        return string_stream.str();
    }

    const xprogressbar& xprogressbar::iter()
    {
        display(false);
        m_progress = -1;

        return *this;
    }

    std::ptrdiff_t xprogressbar::next()
    {
        set_progress(m_progress + 1);
        if (m_progress < m_total)
        {
            return m_progress;
        }
        else
        {
            throw py::stop_iteration();
        }
    }

    std::ptrdiff_t xprogressbar::get_progress() const
    {
        return m_progress;
    }

    void xprogressbar::set_progress(std::ptrdiff_t progress)
    {
        m_progress = progress;
        display(true);
    }

    std::ptrdiff_t xprogressbar::get_total() const
    {
        return m_total;
    }

    void xprogressbar::set_total(std::ptrdiff_t total)
    {
        m_total = total;
        display(true);
    }

    void xprogressbar::display(bool update) const
    {
        auto& interp = xeus::get_interpreter();

        nl::json cpp_transient;
        cpp_transient["display_id"] = m_id;

        nl::json pub_data;
        pub_data["text/html"] = repr_html();
        pub_data["text/plain"] = repr();

        if (!update)
        {
            interp.display_data(
                std::move(pub_data), nl::json::object(), std::move(cpp_transient)
            );
        }
        else
        {
            interp.update_display_data(
                std::move(pub_data), nl::json::object(), std::move(cpp_transient)
            );
        }
    }

    py::object pngxy(const py::object& data)
    {
        py::module builtins = py::module::import("builtins");
        py::module struct_module = py::module::import("struct");

        std::size_t ihdr = data.attr("index")(builtins.attr("bytes")("IHDR", "UTF8")).cast<std::size_t>();

        return struct_module.attr("unpack")(">ii", data[builtins.attr("slice")(ihdr + 4, ihdr + 12)]);
    }

    /******************
     * display module *
     ******************/

    py::module get_display_module_impl()
    {
        py::module display_module = xpyt::create_module("display");

        py::class_<xdisplayhook>(display_module, "DisplayHook")
            .def(py::init<>())
            .def("set_execution_count", &xdisplayhook::set_execution_count)
            .def("__call__", &xdisplayhook::operator(), py::arg("obj"), py::arg("raw") = false);

        display_module.def("display", xdisplay);

        display_module.def("update_display", xupdate_display);

        display_module.def("publish_display_data",
            xpublish_display_data,
            py::arg("data"),
            py::arg("metadata") = py::dict(),
            py::arg("source") = py::str(),
            py::arg("transient") = py::dict());

        display_module.def("clear_output",
            xclear,
            py::arg("wait") = false);

        display_module.def("display_html", xdisplay_html);
        display_module.def("display_markdown", xdisplay_markdown);
        display_module.def("display_svg", xdisplay_svg);
        display_module.def("display_png", xdisplay_png);
        display_module.def("display_jpeg", xdisplay_jpeg);
        display_module.def("display_latex", xdisplay_latex);
        display_module.def("display_json", xdisplay_json);
        display_module.def("display_javascript", xdisplay_javascript);
        display_module.def("display_pdf", xdisplay_pdf);

        py::class_<xdisplay_object>(display_module, "DisplayObject")
            .def(
                py::init<const py::object&, const py::object&, const py::object&, const py::object&>(),
                py::arg("data") = py::none(), py::arg("url") = py::none(), py::arg("filename") = py::none(), py::arg("metadata") = py::none())
            .def("reload", &xdisplay_object::reload)
            .def_property("data", &xdisplay_object::get_data, &xdisplay_object::set_data)
            .def_property("metadata", &xdisplay_object::get_metadata, &xdisplay_object::set_metadata);

        py::class_<xtext_display_object, xdisplay_object>(display_module, "TextDisplayObject")
            .def(
                py::init<const py::object&, const py::object&, const py::object&, const py::object&>(),
                py::arg("data") = py::none(), py::arg("url") = py::none(), py::arg("filename") = py::none(), py::arg("metadata") = py::none());

        py::class_<xhtml, xdisplay_object>(display_module, "HTML")
            .def(
                py::init<const py::object&, const py::object&, const py::object&, const py::object&>(),
                py::arg("data") = py::none(), py::arg("url") = py::none(), py::arg("filename") = py::none(), py::arg("metadata") = py::none())
            .def("_repr_html_", &xhtml::repr_html)
            .def("__html__", &xhtml::html);

        py::class_<xjavascript, xtext_display_object>(display_module, "Javascript")
            .def(
                py::init<const py::object&, const py::object&, const py::object&, const py::object&, const py::object&>(),
                py::arg("data") = py::none(), py::arg("url") = py::none(), py::arg("filename") = py::none(), py::arg("lib") = py::none(), py::arg("css") = py::none())
            .def("_repr_javascript_", &xjavascript::repr_javascript);

        py::class_<xmarkdown, xdisplay_object>(display_module, "Markdown")
            .def(
                py::init<const py::object&, const py::object&, const py::object&, const py::object&>(),
                py::arg("data") = py::none(), py::arg("url") = py::none(), py::arg("filename") = py::none(), py::arg("metadata") = py::none())
            .def("_repr_markdown_", &xmarkdown::repr_markdown);

        py::class_<xmath, xdisplay_object>(display_module, "Math")
            .def(
                py::init<const py::object&, const py::object&, const py::object&, const py::object&>(),
                py::arg("data") = py::none(), py::arg("url") = py::none(), py::arg("filename") = py::none(), py::arg("metadata") = py::none())
            .def("_repr_latex_", &xmath::repr_latex);

        py::class_<xlatex, xdisplay_object>(display_module, "Latex")
            .def(
                py::init<const py::object&, const py::object&, const py::object&, const py::object&>(),
                py::arg("data") = py::none(), py::arg("url") = py::none(), py::arg("filename") = py::none(), py::arg("metadata") = py::none())
            .def("_repr_latex_", &xlatex::repr_latex);

        py::class_<xsvg, xdisplay_object>(display_module, "SVG")
            .def(
                py::init<const py::object&, const py::object&, const py::object&, const py::object&>(),
                py::arg("data") = py::none(), py::arg("url") = py::none(), py::arg("filename") = py::none(), py::arg("metadata") = py::none())
            .def("_repr_svg_", &xsvg::repr_svg);

        py::class_<xjson>(display_module, "JSON")
            .def(
                py::init<const py::object&, const py::object&, const py::object&, const py::bool_&, const py::object&, const py::str&>(),
                py::arg("data") = py::none(), py::arg("url") = py::none(), py::arg("filename") = py::none(),
                py::arg("expanded") = false, py::arg("metadata") = py::none(), py::arg("root") = "root")
            .def("_repr_json_", &xjson::repr_json);

        py::class_<xgeojson, xjson>(display_module, "GeoJSON")
            .def(
                py::init<const py::object&, const py::object&, const py::object&, const py::bool_&, const py::object&, const py::str&, const py::dict&, const py::str&>(),
                py::arg("data") = py::none(), py::arg("url") = py::none(), py::arg("filename") = py::none(),
                py::arg("expanded") = false, py::arg("metadata") = py::none(), py::arg("root") = "root",
                py::arg("layer_options") = py::dict(), py::arg("url_template") = py::str())
            .def("_ipython_display_", &xgeojson::ipython_display);

        py::class_<xprogressbar>(display_module, "ProgressBar")
            .def(py::init<std::ptrdiff_t>(), py::arg("total"))
            .def("__repr__", &xprogressbar::repr)
            .def("_repr_html_", &xprogressbar::repr_html)
            .def("__iter__", &xprogressbar::iter)
            .def("__next__", &xprogressbar::next)
            .def_property("progress", &xprogressbar::get_progress, &xprogressbar::set_progress)
            .def_property("total", &xprogressbar::get_total, &xprogressbar::set_total);

        display_module.def("_pngxy", &pngxy);

        return display_module;
    }

}

namespace xpyt
{
    py::module get_display_module(bool raw_mode /*false*/)
    {
        static py::module display_module;
        if (raw_mode)
        {
            display_module = xpyt_raw::get_display_module_impl();
        }
        else
        {
            display_module = xpyt_ipython::get_display_module_impl();
        }
        return display_module;
    }
}

#ifdef __GNUC__
    #pragma GCC diagnostic pop
#endif
