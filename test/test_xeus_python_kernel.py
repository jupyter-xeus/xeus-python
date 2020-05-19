#############################################################################
# Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and      # 
# Wolf Vollprecht                                                           #
# Copyright (c) 2018, QuantStack                                            #
#                                                                           #
# Distributed under the terms of the BSD 3-Clause License.                  #
#                                                                           #
# The full license is in the file LICENSE, distributed with this software.  #
#############################################################################

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

    def test_xeus_python_missing_magic(self):
        reply, output_msgs = self.execute_helper(code="%missing_magic")
        self.assertRegex(output_msgs[0]['content']['evalue'], "magics not found")

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
        self.assertEqual(
            "In  \033[0;34m[4]\033[0m:\nLine \033[0;34m1\033[0m:     a = []; a.push_back(\x1b[34m3\x1b[39;49;00m)\n",
            traceback[1]
        )
        self.assertEqual(
            "\033[0;31mAttributeError\033[0m: 'list' object has no attribute 'push_back'\n\033[0;31m---------------------------------------------------------------------------\033[0m",
            traceback[2]
        )

if __name__ == '__main__':
    unittest.main()
