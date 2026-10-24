import importlib.util
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
SPEC=importlib.util.spec_from_file_location('diagnostic_oracle',ROOT/'scripts/generate_diagnostic_oracle.py')
ORACLE=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(ORACLE)


class DiagnosticOracleTests(unittest.TestCase):
    def test_fixture_matches_actual_sanitized_c_decoder(self):
        self.assertEqual(ORACLE.FIXTURE.read_bytes(),ORACLE.generate())
