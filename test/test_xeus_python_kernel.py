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

    def run_all_helper(self, codes, timeout=30):
        """Send all execute requests back to back (like "Run All") and
        return the reply status of each request, in order."""
        self.flush_channels()
        ids = [self.kc.execute(code) for code in codes]
        status = {}
        while len(status) < len(ids):
            msg = self.kc.get_shell_msg(timeout=timeout)
            status[msg['parent_header']['msg_id']] = msg['content']['status']
        # consume the iopub messages of these requests (the kernel is shared
        # with the other tests)
        idle = set()
        while idle != set(ids):
            msg = self.kc.get_iopub_msg(timeout=timeout)
            if msg['msg_type'] == 'status' and msg['content']['execution_state'] == 'idle':
                idle.add(msg['parent_header'].get('msg_id'))
        return [status[i] for i in ids]

    def test_run_all_keeps_error(self):
        # the error of the first cell must not be lost when the next
        # execute request arrives before its reply was sent
        status = self.run_all_helper(['1 / 0', 'print("foo")'])
        self.assertEqual(status, ['error', 'ok'])

    def test_run_all_keeps_error_request_while_running(self):
        """The error of a cell must not be lost when the next execute request
        is dispatched after the cell raised but before its reply is sent.

        How the kernel runs a cell (src/xasync_runner.cpp, src/xinterpreter.cpp,
        xeus_python_shell/shell.py):

        1. The asyncio loop's reader callback reads all pending requests
           from the shell socket.
        2. For each request xeus publishes "busy" and "execute_input", then
           calls execute_request_impl, which only schedules the cell as an
           asyncio task and returns.
        3. The task runs the cell. If it raises, _showtraceback stores the
           error in shell.last_error.
        4. After the cell, the task yields twice (asyncio.sleep(0)) and then
           calls the done callback, which reads last_error and sends the
           reply ("ok" or "error").

        The bug: execute_request_impl used to reset last_error in step 2.
        A request dispatched between step 3 and step 4 (during those yields)
        wiped the error and the failing cell was reported as "ok".

        The test forces exactly this order:
        - the first cell blocks the kernel thread with time.sleep (not
          asyncio.sleep: that would let the loop dispatch the next request
          during the sleep, before the error exists, so the old reset would
          not clear anything)
        - the second request is sent only once the first cell is running, so
          it waits unread in the socket and is dispatched in the yields of
          step 4, right after the first cell raised.

        Sending both requests at once does not catch the bug natively: the
        kernel reads both in the same pass of step 1, before the first cell
        runs, so the reset happens before the error is set
        (see test_run_all_keeps_error).

        Old code: ['ok', 'ok'], fixed code: ['error', 'ok'].
        """
        import time  # client side pause (the cell imports its own time)

        # drop leftovers of earlier tests: all messages read below are ours
        self.flush_channels()

        # time.sleep blocks the whole kernel thread for 1s: the event loop
        # cannot read new requests, they queue in the socket. Then 1 / 0
        # raises and sets last_error.
        first = self.kc.execute("import time\ntime.sleep(1)\n1 / 0")

        # "execute_input" is published when the request is dispatched
        # (step 2): the kernel has read the first request and scheduled it
        while True:
            msg = self.kc.get_iopub_msg(timeout=30)
            if msg['msg_type'] == 'execute_input' and msg['parent_header']['msg_id'] == first:
                break

        # give the loop time to start the task: the first cell is now blocked
        # in time.sleep(1). If a very slow machine had not started it yet, the
        # request below would be read before the cell runs and the test would
        # pass even with the old code: a missed detection, never a false failure.
        time.sleep(0.25)

        # waits unread in the socket until time.sleep(1) ends. Then:
        #  1. 1 / 0 raises -> last_error is set
        #  2. the task yields (asyncio.sleep(0)) -> the loop dispatches this
        #     request. Old code: last_error = None. Fixed code: untouched.
        #  3. the done callback of the first cell reads last_error and replies
        #     (old: "ok", fixed: "error", then it clears last_error)
        # This cell waits for the run_cell lock and runs after the first one.
        second = self.kc.execute('print("foo")')

        # collect both execute_reply messages, keyed by the request they
        # answer (independent of reply order); the timeout turns a hanging
        # kernel into a failure instead of a blocked test run
        status = {}
        while len(status) < 2:
            msg = self.kc.get_shell_msg(timeout=30)
            status[msg['parent_header']['msg_id']] = msg['content']['status']

        # the actual check (old code gives ['ok', 'ok'])
        self.assertEqual([status[first], status[second]], ['error', 'ok'])

        # cleanup: the kernel and client are shared by all tests of this class.
        # Consume the iopub messages of both requests (busy, execute_input,
        # error, stream, idle) up to their "idle" status, the last message of
        # a request. Otherwise they leak into the next test (execute_helper
        # checks that the first execute_input it reads is its own).
        # .get: some status messages have an empty parent header.
        idle = set()
        while idle != {first, second}:
            msg = self.kc.get_iopub_msg(timeout=30)
            if msg['msg_type'] == 'status' and msg['content']['execution_state'] == 'idle':
                idle.add(msg['parent_header'].get('msg_id'))


if __name__ == '__main__':
    unittest.main()
