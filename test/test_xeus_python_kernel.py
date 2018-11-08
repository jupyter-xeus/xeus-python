import unittest
import jupyter_kernel_test


class XeusPythonTests(jupyter_kernel_test.KernelTests):

    kernel_name = "xeus-python"
    language_name = "python"

    code_hello_world = "print('hello, world')"

    code_page_something = "?"

    def test_xeus_python_stdout(self):
        reply, output_msgs = self.execute_helper(code='print(3)')
        self.assertEqual(output_msgs[0]['msg_type'], 'stream')
        self.assertEqual(output_msgs[0]['content']['name'], 'stdout')
        self.assertEqual(output_msgs[0]['content']['text'], '3')

    def test_xeus_python_stderr(self):
        reply, output_msgs = self.execute_helper(code='print_err "oops"')
        self.assertEqual(output_msgs[0]['msg_type'], 'error')

if __name__ == '__main__':
    unittest.main()
