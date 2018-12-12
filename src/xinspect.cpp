/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <iostream>

#include "nlohmann/json.hpp"

#include "pybind11/embed.h"
#include "pybind11/functional.h"
#include "pybind11/pybind11.h"

#include "xinput.hpp"
#include "xutils.hpp"

namespace py = pybind11;
using namespace pybind11::literals;

namespace xpyt
{

    PYBIND11_EMBEDDED_MODULE(xeus_python_inspect, m)
    {
        py::module builtins = py::module::import("builtins");
        builtins.attr("exec")(R"(
# Implementation from https://github.com/ipython/ipython/blob/master/IPython/utils/tokenutil.py
from collections import namedtuple
from io import StringIO
from keyword import iskeyword

import tokenize

Token = namedtuple('Token', ['token', 'text', 'start', 'end', 'line'])

def generate_tokens(readline):
    try:
        for token in tokenize.generate_tokens(readline):
            yield token
    except tokenize.TokenError:
        # catch EOF error
        return

def token_at_cursor(cell, cursor_pos=0):
    names = []
    tokens = []
    call_names = []

    offsets = {1: 0} # lines start at 1
    for tup in generate_tokens(StringIO(cell).readline):

        tok = Token(*tup)

        # token, text, start, end, line = tup
        start_line, start_col = tok.start
        end_line, end_col = tok.end
        if end_line + 1 not in offsets:
            # keep track of offsets for each line
            lines = tok.line.splitlines(True)
            for lineno, line in enumerate(lines, start_line + 1):
                if lineno not in offsets:
                    offsets[lineno] = offsets[lineno-1] + len(line)

        offset = offsets[start_line]
        # allow '|foo' to find 'foo' at the beginning of a line
        boundary = cursor_pos + 1 if start_col == 0 else cursor_pos
        if offset + start_col >= boundary:
            # current token starts after the cursor,
            # don't consume it
            break

        if tok.token == tokenize.NAME and not iskeyword(tok.text):
            if names and tokens and tokens[-1].token == tokenize.OP and tokens[-1].text == '.':
                names[-1] = "%s.%s" % (names[-1], tok.text)
            else:
                names.append(tok.text)
        elif tok.token == tokenize.OP:
            if tok.text == '=' and names:
                # don't inspect the lhs of an assignment
                names.pop(-1)
            if tok.text == '(' and names:
                # if we are inside a function call, inspect the function
                call_names.append(names[-1])
            elif tok.text == ')' and call_names:
                call_names.pop(-1)

        tokens.append(tok)

        if offsets[end_line] + end_col > cursor_pos:
            # we found the cursor, stop reading
            break

    if call_names:
        return call_names[-1]
    elif names:
        return names[-1]
    else:
        return ''
        )", m.attr("__dict__"));
    }
}
