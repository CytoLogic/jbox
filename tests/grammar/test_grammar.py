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
    @classmethod
    def setUpClass(cls):
        """Set up test fixtures."""
        test_grammar_path = "gen/bnfc/TestGrammar"
        examples_file = "tests/grammar/shell_examples.txt"
        results_file = "tests/results/grammar/test_grammar_results.txt"

        # Check that TestGrammar executable exists
        cls.assertTrue(
            os.path.exists(test_grammar_path),
            f"TestGrammar executable not found at {test_grammar_path}",
        )

        # Check that examples file exists
        cls.assertTrue(
            os.path.exists(cls.examples_file), f"Examples file not found at {examples_file}"
        )


    @override
    def setUp(self):
        """Set up test fixtures."""

    def test_example(self):
        """Run a shell examples through TestGrammar and save results."""

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
