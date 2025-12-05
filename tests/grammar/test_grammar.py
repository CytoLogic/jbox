#!/usr/bin/env python3

import unittest
import subprocess
import os
from pathlib import Path
from typing import override


def find_proj_root(root_identifier: str = '.git') -> Path | None:
    """Walks up the file tree, Returning a pathlib Path of root_identifier if it is found."""
    current_path = Path.cwd().resolve()
    while True:
        if (current_path / root_identifier).exists():
            return current_path
        else:
            parent_path = current_path.parent
            if parent_path == current_path:
                return None
            current_path = parent_path


class TestShellGrammar(unittest.TestCase):
    """Test shell grammar parsing using TestGrammar executable."""

    @override
    def setUp(self):
        """Set up test fixtures."""
        self.test_grammar_path = "gen/bnfc/TestGrammar"
        self.examples_file = "tests/grammar/shell_examples.txt"
        self.results_file = "tests/grammar/results.txt"

    def test_all_examples(self):
        """Run all shell examples through TestGrammar and save results."""
        # Check that TestGrammar executable exists
        self.assertTrue(
            os.path.exists(self.test_grammar_path),
            f"TestGrammar executable not found at {self.test_grammar_path}",
        )

        # Check that examples file exists
        self.assertTrue(
            os.path.exists(self.examples_file), f"Examples file not found at {self.examples_file}"
        )

        # Read all example lines
        with open(self.examples_file, "r") as f:
            examples = [line.rstrip("\n") for line in f]

        # Open results file for writing
        with open(self.results_file, "w") as results:
            for example in examples:
                # Write input header
                results.write(f"Input: {example}\n")
                results.write("Output: \n")

                # Run TestGrammar with the example as stdin
                try:
                    process = subprocess.run(
                        [self.test_grammar_path],
                        input=example + "\n",
                        capture_output=True,
                        text=True,
                        timeout=5,
                    )

                    # Write the output from TestGrammar
                    results.write(process.stdout)

                    # If there was stderr output, include it
                    if process.stderr:
                        results.write(process.stderr)

                except subprocess.TimeoutExpired:
                    results.write("ERROR: Test timed out\n")
                except Exception as e:
                    results.write(f"ERROR: {str(e)}\n")

                # Add blank lines between test cases
                results.write("\n\n")

        print(f"Results written to {self.results_file}")


if __name__ == "__main__":
    print(f"{Path.cwd()}")
    if (proj_root := find_proj_root()):
        os.chdir(proj_root)
    print(f"{Path.cwd()}")
    unittest.main()
