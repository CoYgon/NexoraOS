#!/usr/bin/env python3
import importlib.util
import unittest
from pathlib import Path

MODULE_PATH = Path(__file__).resolve().parents[1] / "tools" / "nexora_search_bridge.py"
spec = importlib.util.spec_from_file_location("nexora_search_bridge", MODULE_PATH)
bridge = importlib.util.module_from_spec(spec)
assert spec and spec.loader
spec.loader.exec_module(bridge)


class SearchBridgeParserTests(unittest.TestCase):
    def test_extracts_external_result_links(self):
        html = '''<html><body>
        <a href="https://www.google.com/preferences">Settings</a>
        <a href="/url?q=https%3A%2F%2Fexample.org%2Fpage&sa=U"><h3>Example Result</h3></a>
        <a href="https://docs.python.org/3/">Python Documentation</a>
        </body></html>'''
        results = bridge.parse_google_results(html)
        self.assertEqual(len(results), 2)
        self.assertEqual(results[0][0], "Example Result")
        self.assertEqual(results[0][1], "https://example.org/page")
        self.assertEqual(results[1][1], "https://docs.python.org/3/")

    def test_skips_google_internal_links(self):
        html = '<a href="https://www.google.com/search?q=x">Google Search</a>'
        self.assertEqual(bridge.parse_google_results(html), [])

    def test_clean_text(self):
        self.assertEqual(bridge.clean_text("  A &amp; B\n C  "), "A & B C")


if __name__ == "__main__":
    unittest.main(verbosity=2)
