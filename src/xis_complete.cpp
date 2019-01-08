/***************************************************************************
* Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay and      *
* Wolf Vollprecht                                                          *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include "pybind11/pybind11.h"

#include "xutils.hpp"

namespace py = pybind11;

namespace xpyt
{

    /*********************
     * completion module *
     *********************/

    py::module get_completion_module_impl()
    {
        py::module completion_module("completion");

        exec(py::str(R"(
# Implementation from https://github.com/ipython/ipython/blob/master/IPython/core/inputtransformer2.py
import sys
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

def _extract_token(token, tokens_by_line, parenlev):
    tokens_by_line[-1].append(token)
    if (token.type == tokenize.NEWLINE) \
            or ((token.type == tokenize.NL) and (parenlev <= 0)):
        tokens_by_line.append([])
    elif token.string in {'(', '[', '{'}:
        parenlev += 1
    elif token.string in {')', ']', '}'}:
        if parenlev > 0:
            parenlev -= 1

if sys.version_info.major == 3:
    def _gen_tokens(lines, tokens_by_line, parenlev):
        for token in tokenize.generate_tokens(iter(lines).__next__):
            _extract_token(token, tokens_by_line, parenlev)
else:
    class Token():
        def __init__(self, token_tuple):
            self.type = token_tuple[0]
            self.string = token_tuple[1]
            self.start = token_tuple[2]
            self.end = token_tuple[3]
            self.line = token_tuple[4]

    def _gen_tokens(lines, tokens_by_line, parenlev):
        for token_tuple in tokenize.generate_tokens(iter(lines).next):
            token = Token(token_tuple)
            _extract_token(token, tokens_by_line, parenlev)

def make_tokens_by_line(lines):
    tokens_by_line = [[]]
    if len(lines) > 1 and not lines[0].endswith(('\n', '\r', '\r\n', '\x0b', '\x0c')):
        warnings.warn("`make_tokens_by_line` received a list of lines which do not have lineending markers ('\\n', '\\r', '\\r\\n', '\\x0b', '\\x0c'), behavior will be unspecified")
    parenlev = 0
    try:
        _gen_tokens(lines, tokens_by_line, parenlev)
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

    lines = cell.splitlines(True)

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
        )"), completion_module.attr("__dict__"));

        return completion_module;
    }

    py::module get_completion_module()
    {
        static py::module completion_module = get_completion_module_impl();
        return completion_module;
    }
}
