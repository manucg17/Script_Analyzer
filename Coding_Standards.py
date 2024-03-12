import re
from pathlib import Path

class ScriptAnalyzer:
    def __init__(self, script_path):
        self.script_path = Path(script_path)

    def run_analysis(self):
        try:
            # Check script indentation
            self.check_indentation()

            # Check naming conventions
            self.check_naming_conventions()

            # Check modularization
            self.check_modularization()

            # Check file encoding
            self.check_file_encoding()

            # Check consistency
            self.check_consistency()

            # Check code reuse
            self.check_code_reuse()

            # Check performance considerations
            self.check_performance()

            # Check whitespace usage
            self.check_whitespace()

            # Check memory leakages (for C scripts)
            if str(self.script_path).endswith(".c"):
                self.check_memory_leaks()
            elif str(self.script_path).endswith(".pl"):
                self.check_perl_specific_checks()

            print("Script analysis completed successfully.")
            
        except Exception as e:
            print(f"Error during analysis: {str(e)}")


    def check_indentation(self):
        try:
            with open(self.script_path, "r") as script_file:
                lines = script_file.readlines()

                inside_function = False
                indentation_level = 0

                for line_number, line in enumerate(lines, start=1):
                    # Skip empty lines and user comments
                    if not line.strip() or line.strip().startswith("//") or line.strip().startswith("/*"):
                        continue

                    # Check #include lines
                    if line.strip().startswith("#include") and line.strip() != "#include":
                        print(f"Indentation issue at line {line_number}: Incorrect indentation for #include.")

                    # Check for global variables
                    if ";" in line and not inside_function:
                        if line.strip().endswith(";"):
                            print(f"Indentation issue at line {line_number}: Incorrect indentation for global variable declaration.")

                    # Check for function definitions
                    if "{" in line and "(" in line:
                        inside_function = True
                        if line.strip().endswith("{"):
                            indentation_level += 1

                    if inside_function:
                        # Check control structures indentation
                        control_structures = ["for", "while", "do", "if", "else", "elseif", "unless"]
                        for control_structure in control_structures:
                            if control_structure in line.strip():
                                if not line.strip().startswith(" " * 4 * (indentation_level + 1)):
                                    print(f"Indentation issue at line {line_number}: Incorrect indentation for {control_structure} statement.")
                        if "}" in line:
                            indentation_level -= 1
                            if indentation_level == 0:
                                inside_function = False
                                continue

                    # Check indentation of non-empty, non-comment lines
                    if not line.strip().startswith(" " * 4 * indentation_level):
                        print(f"Indentation issue at line {line_number}: Incorrect indentation.")

        except FileNotFoundError:
            print(f"File not found: {self.script_path}")
        except Exception as e:
            print(f"Error during indentation check: {str(e)}")


    def check_naming_conventions(self):
        try:
            with open(self.script_path, "r") as script_file:
                lines = script_file.readlines()

            for line_number, line in enumerate(lines, start=1):
                # Remove leading/trailing whitespaces
                stripped_line = line.strip()

                # Check naming conventions based on file extension
                if str(self.script_path).endswith((".c", ".pl")):
                    self.check_common_naming_conventions(stripped_line, line_number)

        except FileNotFoundError:
            print(f"File not found: {self.script_path}")
        except Exception as e:
            print(f"Error during naming conventions check: {str(e)}")


    def check_common_naming_conventions(self, line, line_number):
        # Implement common naming conventions checks
        # Example: Check if variable names follow a consistent pattern
        if str(self.script_path).endswith(".c"):
            # Check for C-specific naming conventions
            if re.match(r'^[a-zA-Z_][a-zA-Z0-9_]*$', line):
                print(f"Naming convention issue at line {line_number}: {line}")
        elif str(self.script_path).endswith(".pl"):
            # Check for Perl-specific naming conventions
            if re.match(r'^[a-zA-Z][a-zA-Z0-9]*$', line):
                print(f"Naming convention issue at line {line_number}: {line}")


    def check_modularization(self):
        try:
            with open(self.script_path, "r") as script_file:
                lines = script_file.readlines()

            if str(self.script_path).endswith(".c"):
                num_functions = 0
                for line in lines:
                    # Exclude the main function from the count
                    if re.match(r'^[a-zA-Z_][a-zA-Z0-9_]*\s*\(\s*\)\s*\{', line) and "main" not in line:
                        num_functions += 1

                if num_functions < 3:
                    print(f"Modularization issue: The file {self.script_path} should have at least 3 functions.")

            elif str(self.script_path).endswith(".pl"):
                num_subroutines = 0
                for line in lines:
                    if re.match(r'^\s*sub\s', line):
                        num_subroutines += 1

                if num_subroutines < 3:
                    print(f"Modularization issue: The file {self.script_path} should have at least 3 subroutines.")

        except FileNotFoundError:
            print(f"File not found: {self.script_path}")
        except Exception as e:
            print(f"Error during modularization check: {str(e)}")

                
    def check_file_encoding(self):
        try:
            with open(self.script_path, "rb") as script_file:
                script_file.read().decode('utf-8')
        except UnicodeDecodeError as e:
            print(f"File encoding issue: {str(e)} at {self.script_path}")


    def check_consistency(self):
        try:
            with open(self.script_path, "r") as script_file:
                lines = script_file.readlines()

                for line_number, line in enumerate(lines, start=1):
                    # Check for consistent line endings (e.g., only '\n', '\r\n', or '\r')
                    if not line.endswith(('\n', '\r\n', '\r')):
                        print(f"Consistency issue at line {line_number}: Inconsistent line ending.")
        except FileNotFoundError:
            print(f"File not found: {self.script_path}")
        except Exception as e:
            print(f"Error during consistency check: {str(e)}")


    def check_code_reuse(self):
        try:
            with open(self.script_path, "r") as script_file:
                lines = script_file.readlines()

                for line_number, line in enumerate(lines, start=1):
                    # Check for code reuse patterns
                    if re.match(r'^\s*(import|require)\s+', line):
                        print(f"Code reuse issue at line {line_number}: Possible code duplication.")
        except FileNotFoundError:
            print(f"File not found: {self.script_path}")
        except Exception as e:
            print(f"Error during code reuse check: {str(e)}")


    def check_performance(self):
        try:
            # Placeholder for performance checks
            pass
        except Exception as e:
            print(f"Error during performance check: {str(e)}")


    def check_whitespace(self):
        try:
            with open(self.script_path, "r") as script_file:
                lines = script_file.readlines()

                for line_number, line in enumerate(lines, start=1):
                    # Check for excessive whitespace within lines
                    if '  ' in line:
                        print(f"Whitespace issue at line {line_number}: Excessive whitespace within line.")
        except FileNotFoundError:
            print(f"File not found: {self.script_path}")
        except Exception as e:
            print(f"Error during whitespace check: {str(e)}")


    def check_memory_leaks(self):
        try:
            # Placeholder for memory leak checks
            pass
        except Exception as e:
            print(f"Error during memory leak check: {str(e)}")


    def check_perl_specific_checks(self):
        try:
            # Placeholder for Perl-specific checks
            pass
        except Exception as e:
            print(f"Error during Perl-specific check: {str(e)}")


if __name__ == "__main__":
    # script_path = input("Enter the path to the script (C or Perl): ")
    script_path = r'D:\Python\Coding_Standards\For_Review\Hello_World.c'
    analyzer = ScriptAnalyzer(script_path)
    analyzer.run_analysis()