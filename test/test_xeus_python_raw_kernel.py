#############################################################################
# Copyright (c) 2018, Martin Renou, Johan Mabille, Sylvain Corlay, and      #
# Wolf Vollprecht                                                           #
# Copyright (c) 2021, QuantStack                                            #
#                                                                           #
# Distributed under the terms of the BSD 3-Clause License.                  #
#                                                                           #
# The full license is in the file LICENSE, distributed with this software.  #
#############################################################################

import unittest
import jupyter_kernel_test

from jupyter_client.manager import start_new_kernel


class XeusPythonRawTests(jupyter_kernel_test.KernelTests):

    kernel_name = "xpython"
    language_name = "python"

    code_hello_world = "print('hello, world')"

    completion_samples = [
        {'text': 'pri', 'matches': {'print'}},
        {'text': 'from sys imp', 'matches': {'import'}},
        {'text': 'se', 'matches': {'set', 'setattr'}},
    ]

    code_inspect_sample = "open"

    @classmethod
    def setUpClass(cls):
        cls.km, cls.kc = start_new_kernel(
            kernel_name=cls.kernel_name,
            extra_arguments=['-m', 'raw']
        )

    def test_xeus_python_stdout(self):
        self.flush_channels()
        reply, output_msgs = self.execute_helper(code='print(3)')
        self.assertEqual(output_msgs[0]['msg_type'], 'stream')
        self.assertEqual(output_msgs[0]['content']['name'], 'stdout')
        self.assertEqual(output_msgs[0]['content']['text'], '3')

    def test_xeus_python_line_magic(self):
        self.flush_channels()
        reply, output_msgs = self.execute_helper(code="%pwd")
        self.assertEqual(output_msgs[0]['msg_type'], 'stream')
        self.assertEqual(
            output_msgs[0]['content']['text'],
            'IPython magics are disabled in raw mode\n'
        )
        # line magic expressions
        reply, output_msgs = self.execute_helper(code="a = %pwd\nassert a")
        self.assertEqual(reply['content']['status'], 'error')

    def test_xeus_python_stderr(self):
        self.flush_channels()
        reply, output_msgs = self.execute_helper(code='a = []; a.push_back(3)')
        self.assertEqual(output_msgs[0]['msg_type'], 'error')
        self.assertEqual(output_msgs[0]['content']['ename'], 'AttributeError')
        self.assertEqual(output_msgs[0]['content']['evalue'],
                         "'list' object has no attribute 'push_back'")
        traceback = output_msgs[0]['content']['traceback']
        self.assertEqual(
            "\033[0;31m---------------------------------------------------------------------------\033[0m\n\033[0;31mAttributeError\033[0m                            Traceback (most recent call last)",
            traceback[0]
        )
        self.assertEqual(
            "\033[0;31mAttributeError\033[0m: 'list' object has no attribute 'push_back'\n\033[0;31m---------------------------------------------------------------------------\033[0m",
            traceback[2]
        )


if __name__ == '__main__':
    unittest.main()
