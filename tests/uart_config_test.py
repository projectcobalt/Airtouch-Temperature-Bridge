"""Check UART requirements through real ESPHome final configuration validation."""

from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory
import unittest


ROOT = Path(__file__).resolve().parents[1]
BASE_CONFIG = (ROOT / "tests" / "configs" / "common.yaml").read_text().replace(
    "path: ../../components", f'path: "{(ROOT / "components").as_posix()}"'
)


class UARTConfigTests(unittest.TestCase):
    def test_uart_requirements(self):
        cases = (
            ("valid", BASE_CONFIG, None),
            ("baud", BASE_CONFIG.replace("baud_rate: 115200", "baud_rate: 9600"), "requires baud rate 115200"),
            ("data", BASE_CONFIG.replace("baud_rate: 115200", "baud_rate: 115200\n  data_bits: 7"), "requires 8 data bits"),
            ("parity", BASE_CONFIG.replace("baud_rate: 115200", "baud_rate: 115200\n  parity: EVEN"), "requires parity NONE"),
            ("stop", BASE_CONFIG.replace("baud_rate: 115200", "baud_rate: 115200\n  stop_bits: 2"), "requires 1 stop bits"),
            ("rx", BASE_CONFIG.replace("  rx_pin: GPIO7\n", ""), "to declare a rx_pin"),
            ("tx", BASE_CONFIG.replace("  tx_pin: GPIO5\n", ""), "to declare a tx_pin"),
        )
        with TemporaryDirectory(prefix="bridge-uart-test-") as directory:
            for name, config, error in cases:
                with self.subTest(name=name):
                    path = Path(directory) / f"{name}.yaml"
                    path.write_text(config)
                    result = subprocess.run(
                        [sys.executable, "-m", "esphome", "config", str(path)],
                        capture_output=True,
                        text=True,
                        check=False,
                    )
                    output = result.stdout + result.stderr
                    if error is None:
                        self.assertEqual(result.returncode, 0, output)
                    else:
                        self.assertNotEqual(result.returncode, 0, output)
                        self.assertIn(error, output)


if __name__ == "__main__":
    unittest.main()
