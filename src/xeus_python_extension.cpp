/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include "pybind11/pybind11.h"
#include "pybind11/embed.h"

#include "nlohmann/json.hpp"

#include "xeus/xinterpreter.hpp"
#include "xeus/xinput.hpp"

#include "xutils.hpp"

namespace py = pybind11;
namespace nl = nlohmann;

namespace xpyt
{
    /***********************
     * xstream declaration *
     ***********************/

    class xstream
    {
    public:

        xstream(std::string stream_name);
        virtual ~xstream();

        void write(const std::string& message);

    private:

        std::string m_stream_name;
    };

    /**************************
     * xstream implementation *
     **************************/

    xstream::xstream(std::string stream_name)
        : m_stream_name(stream_name)
    {
    }

    xstream::~xstream()
    {
    }

    void xstream::write(const std::string& message)
    {
        xeus::get_interpreter().publish_stream(m_stream_name, message);
    }

    std::string input(const std::string& prompt)
    {
        return xeus::blocking_input_request(prompt, false);
    }

    std::string getpass(const std::string& prompt)
    {
        return xeus::blocking_input_request(prompt, true);
    }

    void notimplemented(const std::string&)
    {
        throw std::runtime_error("This frontend does not support input requests");
    }

    /*********************
     * xcomm declaration *
     *********************/

    class xcomm
    {
    public:

        using python_callback_type = std::function<void(py::object)>;
        using cpp_callback_type = std::function<void(const xeus::xmessage&)>;
        using zmq_buffers_type = std::vector<zmq::message_t>;

        xcomm(const py::args& args, const py::kwargs& kwargs);
        xcomm(xeus::xcomm&& comm);
        xcomm(xcomm&& comm) = default;
        virtual ~xcomm();

        std::string comm_id() const;
        bool kernel() const;

        void close(const py::args& args, const py::kwargs& kwargs);
        void send(const py::args& args, const py::kwargs& kwargs);
        void on_msg(const python_callback_type& callback);
        void on_close(const python_callback_type& callback);

    private:

        xeus::xtarget* target(const py::kwargs& kwargs) const;
        xeus::xguid id(const py::kwargs& kwargs) const;
        cpp_callback_type cpp_callback(const python_callback_type& callback) const;

        xeus::xcomm m_comm;
    };

    /************************
     * xcomm implementation *
     ************************/

    xcomm::xcomm(const py::args& /*args*/, const py::kwargs& kwargs)
        : m_comm(target(kwargs), id(kwargs))
    {
        m_comm.open(
            kwargs.attr("get")("metadata", py::dict()),
            kwargs.attr("get")("data", py::dict()),
            pylist_to_zmq_buffers(kwargs.attr("get")("buffers", py::list()))
        );
    }

    xcomm::xcomm(xeus::xcomm&& comm)
        : m_comm(std::move(comm))
    {
    }

    xcomm::~xcomm()
    {
    }

    std::string xcomm::comm_id() const
    {
        return m_comm.id();
    }

    bool xcomm::kernel() const
    {
        return true;
    }

    void xcomm::close(const py::args& /*args*/, const py::kwargs& kwargs)
    {
        m_comm.close(
            kwargs.attr("get")("metadata", py::dict()),
            kwargs.attr("get")("data", py::dict()),
            pylist_to_zmq_buffers(kwargs.attr("get")("buffers", py::list()))
        );
    }

    void xcomm::send(const py::args& /*args*/, const py::kwargs& kwargs)
    {
        m_comm.send(
            kwargs.attr("get")("metadata", py::dict()),
            kwargs.attr("get")("data", py::dict()),
            pylist_to_zmq_buffers(kwargs.attr("get")("buffers", py::list()))
        );
    }

    void xcomm::on_msg(const python_callback_type& callback)
    {
        m_comm.on_message(cpp_callback(callback));
    }

    void xcomm::on_close(const python_callback_type& callback)
    {
        m_comm.on_close(cpp_callback(callback));
    }

    xeus::xtarget* xcomm::target(const py::kwargs& kwargs) const
    {
        std::string target_name = kwargs["target_name"].cast<std::string>();
        return xeus::get_interpreter().comm_manager().target(target_name);
    }

    xeus::xguid xcomm::id(const py::kwargs& kwargs) const
    {
        if (py::hasattr(kwargs, "comm_id"))
        {
            // TODO: prevent copy
            return xeus::xguid(kwargs["comm_id"].cast<std::string>());
        }
        else
        {
            return xeus::new_xguid();
        }
    }

    auto xcomm::cpp_callback(const python_callback_type& py_callback) const -> cpp_callback_type
    {
        return [this, py_callback](const xeus::xmessage& msg) {
            py_callback(cppmessage_to_pymessage(msg));
        };
    }

    void register_post_execute(py::args, py::kwargs)
    {
    }

    void enable_gui(py::args, py::kwargs)
    {
    }

    void register_target(const py::str& target_name, const py::object& callback)
    {
        auto target_callback = [&callback] (xeus::xcomm&& comm, const xeus::xmessage& msg) {
            callback(xcomm(std::move(comm)), cppmessage_to_pymessage(msg));
        };

        xeus::get_interpreter().comm_manager().register_comm_target(
            static_cast<std::string>(target_name), target_callback
        );
    }

    namespace detail
    {
        struct xmock_object
        {
        };
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

    nl::json mime_bundle_repr(const py::object& obj)
    {
        py::module py_json = py::module::import("json");
        py::module builtins = py::module::import(XPYT_BUILTINS);
        nl::json pub_data;

        if (hasattr(obj, "_repr_mimebundle_"))
        {
            pub_data = obj.attr("_repr_mimebundle_")();
        }
        else
        {
            if (hasattr(obj, "_repr_html_"))
            {
                pub_data["text/html"] = py::str(obj.attr("_repr_html_")());
            }
            if (hasattr(obj, "_repr_json_"))
            {
                pub_data["application/json"] = py::str(obj.attr("_repr_json_")());
            }
            if (hasattr(obj, "_repr_jpeg_"))
            {
                pub_data["image/jpeg"] = py::str(obj.attr("_repr_jpeg_")());
            }
            if (hasattr(obj, "_repr_png_"))
            {
                pub_data["image/png"] = py::str(obj.attr("_repr_png_")());
            }
            if (hasattr(obj, "_repr_svg_"))
            {
                pub_data["image/svg+xml"] = py::str(obj.attr("_repr_svg_")());
            }
            if (hasattr(obj, "_repr_latex_"))
            {
                pub_data["text/latex"] = py::str(obj.attr("_repr_latex_")());
            }

            pub_data["text/plain"] = py::str(builtins.attr("repr")(obj));
        }

        return pub_data;
    }

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

            nl::json pub_data;
            if (raw)
            {
                pub_data = obj;
            }
            else
            {
                pub_data = mime_bundle_repr(obj);
            }

            interp.publish_execution_result(
                m_execution_count,
                std::move(pub_data),
                nl::json::object()
            );
        }
    }

    /***************************
     * xdisplay implementation *
     ***************************/

    void xdisplay(const py::object& obj, const py::object display_id, bool update, bool raw)
    {
        auto& interp = xeus::get_interpreter();

        if (!obj.is_none())
        {
            if (hasattr(obj, "_ipython_display_"))
            {
                obj.attr("_ipython_display_")();
                return;
            }

            nl::json pub_data;
            if (raw)
            {
                pub_data = obj;
            }
            else
            {
                pub_data = mime_bundle_repr(obj);
            }

            nl::json transient = nl::json::object();
            if (!display_id.is_none())
            {
                transient["display_id"] = display_id;
            }
            if (update)
            {
                interp.update_display_data(
                    std::move(pub_data), nl::json::object(), std::move(transient));
            }
            else
            {
                interp.display_data(std::move(pub_data), nl::json::object(), std::move(transient));
            }
        }
    }

    /********************************
     * xeus_python_extension module *
     ********************************/

    PYBIND11_MODULE(xeus_python_extension, m)
    {
        py::class_<xstream>(m, "XPythonStream")
            .def(py::init<std::string>())
            .def("write", &xstream::write);

        m.def("input", input, py::arg("prompt") = "")
            .def("getpass", getpass, py::arg("prompt") = "")
            .def("notimplemented", notimplemented, py::arg("prompt") = "");

#if PY_MAJOR_VERSION == 2
        m.def("raw_input", input, py::arg("prompt") = "");
#endif

        py::class_<detail::xmock_object> _Mock(m, "_Mock");

        py::class_<xcomm>(m, "XPythonComm")
            .def(py::init<py::args, py::kwargs>())
            .def("close", &xcomm::close)
            .def("send", &xcomm::send)
            .def("on_msg", &xcomm::on_msg)
            .def("on_close", &xcomm::on_close)
            .def_property_readonly("comm_id", &xcomm::comm_id)
            .def_property_readonly("kernel", &xcomm::kernel);

        m.def("register_target", &register_target);
        m.def("register_post_execute", &register_post_execute);
        m.def("enable_gui", &enable_gui);

        m.def("get_kernel", [m]() {
            py::object xeus_python = m.attr("_Mock");
            py::object kernel = m.attr("_Mock");
            py::object comm_manager = m.attr("_Mock");

            xeus_python.attr("register_post_execute") = m.attr("register_post_execute");
            xeus_python.attr("enable_gui") = m.attr("enable_gui");
            comm_manager.attr("register_target") = m.attr("register_target");
            kernel.attr("comm_manager") = comm_manager;
            xeus_python.attr("kernel") = kernel;
            return xeus_python;
        });

        py::class_<xdisplayhook>(m, "XPythonDisplay")
            .def(py::init<>())
            .def("set_execution_count", &xdisplayhook::set_execution_count)
            .def("__call__", &xdisplayhook::operator(), py::arg("obj"), py::arg("raw") = false);

        m.def("display",
              xdisplay,
              py::arg("obj"),
              py::arg("display_id") = py::none(),
              py::arg("update") = false,
              py::arg("raw") = false);

        m.def("update_display",
              xdisplay,
              py::arg("obj"),
              py::arg("display_id"),
              py::arg("update") = true,
              py::arg("raw") = false);

        py::module builtins = py::module::import(XPYT_BUILTINS);
        builtins.attr("exec")(R"(
# Implementation from https://github.com/ipython/ipython/blob/master/IPython/core/inputtransformer2.py
import re
import tokenize
import warnings
from codeop import compile_command

_indent_re = re.compile(r'^[ \t]+')

def find_last_indent(lines):
    m = _indent_re.match(lines[-1])
    if not m:
        return 0
    return len(m.group(0).replace('\t', ' '*4))

def leading_indent(lines):
    if not lines:
        return lines
    m = _indent_re.match(lines[0])
    if not m:
        return lines
    space = m.group(0)
    n = len(space)
    return [l[n:] if l.startswith(space) else l
            for l in lines]

class PromptStripper:
    def __init__(self, prompt_re, initial_re=None):
        self.prompt_re = prompt_re
        self.initial_re = initial_re or prompt_re

    def _strip(self, lines):
        return [self.prompt_re.sub('', l, count=1) for l in lines]

    def __call__(self, lines):
        if not lines:
            return lines
        if self.initial_re.match(lines[0]) or \
                (len(lines) > 1 and self.prompt_re.match(lines[1])):
            return self._strip(lines)
        return lines

classic_prompt = PromptStripper(
    prompt_re=re.compile(r'^(>>>|\.\.\.)( |$)'),
    initial_re=re.compile(r'^>>>( |$)')
)

interactive_prompt = PromptStripper(re.compile(r'^(In \[\d+\]: |\s*\.{3,}: ?)'))

def make_tokens_by_line(lines):
    NEWLINE, NL = tokenize.NEWLINE, tokenize.NL
    tokens_by_line = [[]]
    if len(lines) > 1 and not lines[0].endswith(('\n', '\r', '\r\n', '\x0b', '\x0c')):
        warnings.warn("`make_tokens_by_line` received a list of lines which do not have lineending markers ('\\n', '\\r', '\\r\\n', '\\x0b', '\\x0c'), behavior will be unspecified")
    parenlev = 0
    try:
        for token in tokenize.generate_tokens(iter(lines).__next__):
            tokens_by_line[-1].append(token)
            if (token.type == NEWLINE) \
                    or ((token.type == NL) and (parenlev <= 0)):
                tokens_by_line.append([])
            elif token.string in {'(', '[', '{'}:
                parenlev += 1
            elif token.string in {')', ']', '}'}:
                if parenlev > 0:
                    parenlev -= 1
    except tokenize.TokenError:
        # Input ended in a multiline string or expression. That's OK for us.
        pass

    if not tokens_by_line[-1]:
        tokens_by_line.pop()

    return tokens_by_line

def check_complete(cell):
    # Remember if the lines ends in a new line.
    ends_with_newline = False
    for character in reversed(cell):
        if character == '\n':
            ends_with_newline = True
            break
        elif character.strip():
            break
        else:
            continue

    if not ends_with_newline:
        cell += '\n'

    lines = cell.splitlines(keepends=True)

    if not lines:
        return 'complete', None

    if lines[-1].endswith('\\'):
        # Explicit backslash continuation
        return 'incomplete', find_last_indent(lines)

    cleanup_transforms = [
        leading_indent,
        classic_prompt,
        interactive_prompt,
    ]
    try:
        for transform in cleanup_transforms:
            lines = transform(lines)
    except SyntaxError:
        return 'invalid', None

    if lines[0].startswith('%%'):
        # Special case for cell magics - completion marked by blank line
        if lines[-1].strip():
            return 'incomplete', find_last_indent(lines)
        else:
            return 'complete', None

    tokens_by_line = make_tokens_by_line(lines)

    if not tokens_by_line:
        return 'incomplete', find_last_indent(lines)

    if tokens_by_line[-1][-1].type != tokenize.ENDMARKER:
        # We're in a multiline string or expression
        return 'incomplete', find_last_indent(lines)

    newline_types = {tokenize.NEWLINE, tokenize.COMMENT, tokenize.ENDMARKER}

    # Pop the last line which only contains DEDENTs and ENDMARKER
    last_token_line = None
    if {t.type for t in tokens_by_line[-1]} in [
        {tokenize.DEDENT, tokenize.ENDMARKER},
        {tokenize.ENDMARKER}
    ] and len(tokens_by_line) > 1:
        last_token_line = tokens_by_line.pop()

    while tokens_by_line[-1] and tokens_by_line[-1][-1].type in newline_types:
        tokens_by_line[-1].pop()

    if len(tokens_by_line) == 1 and not tokens_by_line[-1]:
        return 'incomplete', 0

    if tokens_by_line[-1][-1].string == ':':
        # The last line starts a block (e.g. 'if foo:')
        ix = 0
        while tokens_by_line[-1][ix].type in {tokenize.INDENT, tokenize.DEDENT}:
            ix += 1

        indent = tokens_by_line[-1][ix].start[1]
        return 'incomplete', indent + 4

    if tokens_by_line[-1][0].line.endswith('\\'):
        return 'incomplete', None

    # At this point, our checks think the code is complete (or invalid).
    # We'll use codeop.compile_command to check this with the real parser
    try:
        with warnings.catch_warnings():
            warnings.simplefilter('error', SyntaxWarning)
            res = compile_command(''.join(lines), symbol='exec')
    except (SyntaxError, OverflowError, ValueError, TypeError,
            MemoryError, SyntaxWarning):
        return 'invalid', None
    else:
        if res is None:
            return 'incomplete', find_last_indent(lines)

    if last_token_line and last_token_line[0].type == tokenize.DEDENT:
        if ends_with_newline:
            return 'complete', None
        return 'incomplete', find_last_indent(lines)

    # If there's a blank line at the end, assume we're ready to execute
    if not lines[-1].strip():
        return 'complete', None

    return 'complete', None
        )", m.attr("__dict__"));
    }
}
