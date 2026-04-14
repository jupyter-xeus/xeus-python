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
from jupyter_client.manager import start_new_kernel
import textwrap

class XeusPythonTests(jupyter_kernel_test.KernelTests):

    kernel_name = "xpython"
    language_name = "python"

    code_hello_world = "print('hello, world')"

    code_page_something = "?print"

    completion_samples = [
        {'text': 'pri', 'matches': {'print'}},
        {'text': 'from sys imp', 'matches': {'import ', 'import'}}
    ]

    complete_code_samples = ['1', "print('hello, world')", "def f(x):\n  return x*2\n\n\n"]
    incomplete_code_samples = ["print('''hello", "def f(x):\n  x*2"]
    invalid_code_samples = ['import = 7q']

    code_inspect_sample = "open"

    def test_xeus_python_history_manager(self):
        reply, output_msgs = self.execute_helper(
            code="assert get_ipython().history_manager is not None"
        )
        self.assertEqual(reply['content']['status'], 'ok')

    def test_xeus_python_stdout(self):
        reply, output_msgs = self.execute_helper(code='print(3)')
        self.assertEqual(output_msgs[0]['msg_type'], 'stream')
        self.assertEqual(output_msgs[0]['content']['name'], 'stdout')
        self.assertEqual(output_msgs[0]['content']['text'], '3')

    def test_xeus_python_stderr(self):
        reply, output_msgs = self.execute_helper(code='a = []; a.push_back(3)')
        self.assertEqual(output_msgs[0]['msg_type'], 'error')
    
    def test_toplevel_await(self):
        code =  textwrap.dedent(R"""
        import asyncio
        async def f():
            await asyncio.sleep(0.25)
            print("World")
        print("Hello")
        await f()
        print("!")
        """)
        reply, output_msgs = self.execute_helper(code=code)
        self.assertEqual(reply['content']['status'], 'ok')
        import json
        print(json.dumps(output_msgs, indent=2, default=str))

        def checkMsg(msg, text):
            self.assertEqual(msg['msg_type'], 'stream')
            self.assertEqual(msg['content']['name'], 'stdout')
            self.assertEqual(msg['content']['text'], text)

        checkMsg(output_msgs[0], 'Hello')
        checkMsg(output_msgs[1], '\n')  # The newline after "Hello"
        checkMsg(output_msgs[2], 'World')
        checkMsg(output_msgs[3], '\n')  # The newline after "World"
        checkMsg(output_msgs[4], '!')


if __name__ == '__main__':
    unittest.main()
