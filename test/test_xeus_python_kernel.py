#############################################################################
# Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and      # 
# Wolf Vollprecht                                                           #
# Copyright (c) 2018, QuantStack                                            #
#                                                                           #
# Distributed under the terms of the BSD 3-Clause License.                  #
#                                                                           #
# The full license is in the file LICENSE, distributed with this software.  #
#############################################################################

import tempfile
import unittest
import jupyter_kernel_test


class XeusPythonTests(jupyter_kernel_test.KernelTests):

    kernel_name = "xpython"
    language_name = "python"

    code_hello_world = "print('hello, world')"

    code_page_something = "?print"

    completion_samples = [
        {'text': 'pri', 'matches': {'print'}},
        {'text': 'from sys imp', 'matches': {'import'}},
        {'text': 'se', 'matches': {'set', 'setattr'}},
    ]

    complete_code_samples = ['1', "print('hello, world')", "def f(x):\n  return x*2\n\n\n"]
    incomplete_code_samples = ["print('''hello", "def f(x):\n  x*2"]
    invalid_code_samples = ['import = 7q']

    code_inspect_sample = "open"

    def test_xeus_python_line_magic(self):
        reply, output_msgs = self.execute_helper(code="%pwd")
        self.assertEqual(output_msgs[0]['msg_type'], 'execute_result')
        self.assertEqual(reply['content']['status'], 'ok')

        # line magic expressions
        reply, output_msgs = self.execute_helper(code="a = %pwd\nassert a")
        self.assertEqual(reply['content']['status'], 'ok')
        self.assertFalse(output_msgs)

    def test_xeus_python_cell_magic(self):
        """Test calling cell magic"""
        with tempfile.NamedTemporaryFile(delete=False) as tmpfile:
            temp_filename = tmpfile.name
        reply, output_msgs = self.execute_helper(code="%%writefile {}\ntest file".format(temp_filename))
        # we don't care about the actual function of the magic
        # only check if execution succeeded
        self.assertEqual(reply['content']['status'], 'ok')

    def test_xeus_python_shell_magic(self):
        """Test calling shell escape (! or !!) magic."""

        # the shell command does not exist, but the magic execution should succeed
        reply, output_msgs = self.execute_helper(code="!does_not_exist -params")
        self.assertEqual(reply['content']['status'], 'ok')

        reply, output_msgs = self.execute_helper(code="!!does_not_exist -params")
        self.assertEqual(reply['content']['status'], 'ok')

    def test_xeus_python_user_defined_magics(self):
        """Test user-defined magic."""

        magic_def_code = """
        from IPython.core.magic import register_line_magic, register_cell_magic
        @register_line_magic
        def lmagic(line):
            return line
        @register_cell_magic
        def cmagic(line, body):
            return body"""

        reply, output_msgs = self.execute_helper(code=magic_def_code)
        self.assertEqual(reply['content']['status'], 'ok')

        reply, output_msgs = self.execute_helper(code="%lmagic hello")
        self.assertEqual(reply['content']['status'], 'ok')
        self.assertTrue('hello' in output_msgs[0]['content']['data']['text/plain'])

        # cant call cell magic as line magic
        reply, output_msgs = self.execute_helper(code="%cmagic hello")
        self.assertEqual(reply['content']['status'], 'error')

        reply, output_msgs = self.execute_helper(code="%%cmagic\nworld")
        self.assertEqual(reply['content']['status'], 'ok')
        self.assertTrue('world' in output_msgs[0]['content']['data']['text/plain'])

        # cant call line magic as as cell magic
        reply, output_msgs = self.execute_helper(code="%%lmagic hello")
        self.assertEqual(reply['content']['status'], 'error')

    def test_xeus_python_missing_magic(self):
        reply, output_msgs = self.execute_helper(code="%missing_magic")
        self.assertRegex(output_msgs[0]['content']['evalue'], "magics not found")

    def test_xeus_python_load_ext_magic(self):
        """Test loading extensions"""
        reply, output_msgs = self.execute_helper(code="%load_ext example_magic")
        reply, output_msgs = self.execute_helper(code="%abra line")
        self.assertEqual(reply['content']['status'], 'ok')

    def test_xeus_python_history_manager(self):
        reply, output_msgs = self.execute_helper(code="assert get_ipython().history_manager is not None")
        self.assertEqual(reply['content']['status'], 'ok')

    def test_xeus_python_stdout(self):
        reply, output_msgs = self.execute_helper(code='print(3)')
        self.assertEqual(output_msgs[0]['msg_type'], 'stream')
        self.assertEqual(output_msgs[0]['content']['name'], 'stdout')
        self.assertEqual(output_msgs[0]['content']['text'], '3')

    def test_xeus_python_stderr(self):
        reply, output_msgs = self.execute_helper(code='a = []; a.push_back(3)')
        self.assertEqual(output_msgs[0]['msg_type'], 'error')
        self.assertEqual(output_msgs[0]['content']['ename'], 'AttributeError')
        self.assertEqual(output_msgs[0]['content']['evalue'], "'list' object has no attribute 'push_back'")
        traceback = output_msgs[0]['content']['traceback']
        self.assertEqual(
            "\033[0;31m---------------------------------------------------------------------------\033[0m\n\033[0;31mAttributeError\033[0m                            Traceback (most recent call last)",
            traceback[0]
        )
        self.assertTrue(
            "a = []; a.push_back" in traceback[1]
        )
        self.assertEqual(
            "\033[0;31mAttributeError\033[0m: 'list' object has no attribute 'push_back'\n\033[0;31m---------------------------------------------------------------------------\033[0m",
            traceback[2]
        )

if __name__ == '__main__':
    unittest.main()
