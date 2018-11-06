import unittest
import jupyter_kernel_test

class TestDummy(unittest.TestCase):

    def test_dummy(self):
        self.assertEqual(0, 0)


if __name__ == '__main__':
    unittest.main()
