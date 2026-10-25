"""Production AppModel/GaugeCore on macOS, with explicit native/cloud spies."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(sys.platform == 'darwin' and shutil.which('swift'), 'macOS SwiftUI toolchain is required')
class AppModelUXTests(unittest.TestCase):
    def test_demo_composition_and_cancellation(self):
        with tempfile.TemporaryDirectory(prefix='ag-appmodel-ux-') as directory:
            root = Path(directory)
            (root / 'Core').symlink_to(ROOT / 'mobile/Sources/GaugeCore', target_is_directory=True)
            app = root / 'App'; app.mkdir()
            for source in ['AppModel.swift', 'AppAppearance.swift', 'DemoFirmwareState.swift']:
                (app / source).symlink_to(ROOT / 'mobile/App/Model' / source)
            for source in ['QualifiedSessionTestPipe.swift', 'QualifiedSessionTestProvider.swift', 'QualifiedSessionTestStorage.swift']:
                (app / source).symlink_to(ROOT / 'mobile/Tests/GaugeCoreTests' / source)
            (app / 'AppModelDoubles.swift').symlink_to(ROOT / 'mobile/Tests/AppModelHarness/AppModelDoubles.swift')
            tests = root / 'Tests'; tests.mkdir()
            (tests / 'AppModelExperienceTests.swift').symlink_to(ROOT / 'mobile/Tests/AppModelHarness/AppModelExperienceTests.swift')
            (root / 'Package.swift').write_text('''// swift-tools-version: 5.9
import PackageDescription
let package = Package(name: "AppModelHarness", platforms: [.macOS(.v14)], targets: [
    .target(name: "GaugeCore", path: "Core", resources: [.process("Resources")]),
    .target(name: "AppModelHarness", dependencies: ["GaugeCore"], path: "App"),
    .testTarget(name: "AppModelHarnessTests", dependencies: ["AppModelHarness", "GaugeCore"], path: "Tests")
])
''')
            result = subprocess.run(['swift', 'test', '--package-path', str(root)],
                                    capture_output=True, text=True, timeout=180)
            self.assertEqual(result.returncode, 0, (result.stdout + result.stderr)[-16000:])
            self.assertIn('Executed 8 tests, with 0 failures', result.stdout + result.stderr)
