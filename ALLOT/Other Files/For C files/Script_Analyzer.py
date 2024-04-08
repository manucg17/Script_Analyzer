import re
import os
import logging
import subprocess
import smtplib
import platform
from pathlib import Path
from datetime import datetime
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText
from email.mime.base import MIMEBase
from email import encoders
from pycparser import c_ast, parse_file

# Set global configuration values
SMTP_SERVER = 'smtp-mail.outlook.com'
SMTP_PORT = 587

# Set global indentation, line count, and iteration values
INDENTATION_SPACES = 4
EXPECTED_LINE_COUNT = 2000
ITERATION_VALUES = {
    'MAX_FUNCTION_COUNT': 3,  # Maximum number of functions expected in the script
    'MAX_SUBROUTINE_COUNT': 3  # Maximum number of subroutines expected in the script
}

class ScriptAnalyzer:
    def __init__(self, script_path, recipient_email, sender_email, sender_password):
        self.script_path = Path(script_path)
        self.recipient_email = recipient_email
        self.sender_email = sender_email
        self.sender_password = sender_password
        self.log_file = self.get_log_file_name()
        self.counts = {
            'include_directive_check': 0,
            'analysis': 0,
            'total_lines_check': 0,
            'indentation_check': 0,
            'naming_conventions_check': 0,
            'preprocessing': 0,
            'modularization_check': 0,
            'consistency_check': 0,
            'code_reuse_check': 0,
            'repetition_check': 0,
            'excess_whitespace_check': 0
        }
        logging.basicConfig(filename=self.log_file, level=logging.INFO,
                            format='%(asctime)s - %(levelname)s - %(message)s')
        
        # Initialize error count
        self.error_count = 0

    def get_log_file_name(self):
        current_datetime = datetime.now().strftime("%H-%M-%S-on-%d-%m-%Y")
        log_folder = self.script_path.parent / "Logs"
        log_folder.mkdir(parents=True, exist_ok=True)  # Create Logs folder if it doesn't exist
        log_file_name = f"Logs-{self.script_path.stem}-at-{current_datetime}.log"
        return log_folder / log_file_name

    def check_include_directive(self):
        try:
            with open(self.script_path, "r") as script_file:
                lines = script_file.readlines()

                first_non_comment_line = None
                for line_number, line in enumerate(lines, start=1):
                    if not line.strip() or line.strip().startswith("//") or line.strip().startswith("/*"):
                        continue
                    first_non_comment_line = line_number
                    break

                if not first_non_comment_line or not lines[first_non_comment_line - 1].strip().startswith("#include "):
                    logging.error("Mandatory '#include ' directive missing at the beginning of the file.")
                else:
                    logging.error("No issue observed check------------ .")
                    # Remove this line to avoid double counting
                    # self.counts['total_lines_check'] += 1
        except FileNotFoundError:
            logging.error(f"File not found: {self.script_path}")
        except Exception as e:
            logging.error(f"Error during include directive check: {str(e)}")

    def run_analysis(self):
        try:
            # Check for mandatory #include directive
            self.check_include_directive()
            # self.counts['analysis'] += 1

            try:
                # Check script indentation
                self.check_total_lines()
                # self.counts['total_lines_check'] += 1
            except Exception as e:
                logging.error(f"Error during total lines check: {str(e)}")

            try:
                # Check script indentation
                self.check_indentation()
                #self.counts['indentation_check'] += 1
            except Exception as e:
                logging.error(f"Error during indentation check: {str(e)}")

            try:
                # Check naming conventions
                self.check_naming_conventions()
                #self.counts['naming_conventions_check'] += 1
            except Exception as e:
                logging.error(f"Error during naming conventions check: {str(e)}")

            try:
                # Check modularization
                self.check_modularization()
                #self.counts['modularization_check'] += 1
            except Exception as e:
                logging.error(f"Error during modularization check: {str(e)}")

            # Check file encoding
            self.check_file_encoding()

            try:
                # Check consistency
                self.check_consistency()
                # self.counts['consistency_check'] += 1
            except Exception as e:
                logging.error(f"Error during consistency check: {str(e)}")

            try:
                # Check code reuse
                self.check_code_reuse()
                # self.counts['code_reuse_check'] += 1
            except Exception as e:
                logging.error(f"Error during code reuse check: {str(e)}")

            try:
                # Check whitespace usage
                self.check_excess_whitespace()
                # self.counts['whitespace_check'] += 1
            except Exception as e:
                logging.error(f"Error during whitespace check: {str(e)}")
                
            try:
                # Check Repeated lines
                self.check_repetition()
            except Exception as e:
                logging.error(f"Error during whitespace check: {str(e)}")

            # Print summary of the analysis results
            print("Script Analysis completed.")
            # Creating Log with Analysis in Log Directory
            logging.info("Script Analysis completed.")

            # Email the log file
            sender_email = self.sender_email
            sender_password = self.sender_password
            recipient_email = self.recipient_email
            attachment_path = self.log_file
            send_email(sender_email, sender_password, recipient_email, attachment_path, self.counts)

        except Exception as e:
            logging.error(f"Error during analysis: {str(e)}")
            self.error_count += 1
            logging.error(f"Error count: {self.error_count}")  # Log the error count

    def check_total_lines(self):
        try:
            with open(self.script_path, "r") as script_file:
                lines = script_file.readlines()
                total_lines = len(lines)
                if total_lines > EXPECTED_LINE_COUNT:
                    logging.warning(f"Total number of lines ({total_lines}) exceeds the recommended maximum of 2000 lines.")
                    self.counts['total_lines_check'] += 1
            logging.info(f"Total lines check completed - Count: {self.counts['total_lines_check']}")
        except FileNotFoundError:
            logging.error(f"File not found: {self.script_path}")
        except Exception as e:
            logging.error(f"Error during total lines check: {str(e)}")

    def check_indentation(self):
        try:
            with open(self.script_path, "r") as script_file:
                lines = script_file.readlines()

                inside_function = False
                indentation_level = 0

                for line_number, line in enumerate(lines, start=1):
                    if not line.strip() or line.strip().startswith("//") or line.strip().startswith("/*"):
                        continue

                    if "\t" in line:
                        logging.warning(f"Indentation issue at line {line_number}: TAB space used. Convert TABs to spaces.")
                        self.counts['indentation_check'] += 1

                    if line.strip().startswith("#include"):
                        if not re.match(r'^#include\s*$', line.strip()):
                            logging.warning(f"Syntax issue at line {line_number}: Incorrect syntax - Include.")
                        continue
                    
                    if line.strip().startswith("Using"):
                        if not re.match(r'^Using\s*$', line.strip()):
                            logging.warning(f"Syntax issue at line {line_number}: Incorrect syntax - Using.")
                        continue

                    if line.strip().startswith("typedef"):
                        if not re.match(r'^typedef\s*$', line.strip()):
                            logging.warning(f"Syntax issue at line {line_number}: Incorrect syntax - Typedef.")
                        continue

                    if "{" in line and "(" in line and not inside_function:
                        if line.strip().endswith("{"):
                            inside_function = True
                            continue

                    if inside_function:
                        control_structures = ["if", "else if", "else", "switch", "for", "while", "do", "case", "default"]
                        for control_structure in control_structures:
                            if control_structure in line.strip():
                                if not line.startswith(" " * INDENTATION_SPACES * indentation_level):
                                    logging.warning(f"Indentation issue at line {line_number}: Incorrect indentation for {control_structure} statement.")
                                    self.counts['indentation_check'] += 1
                                if line.strip().endswith("{"):
                                    indentation_level += 1
                            if line.strip().startswith("}"):
                                indentation_level -= 1
                                if indentation_level == 0:
                                    inside_function = False
                                    continue

                    if "do" in line.strip() and "{" in line.strip():
                        indentation_level += 1
                    if "while" in line.strip() and ";" in line.strip() and "do" not in line.strip():
                        indentation_level -= 1

                    if not line.startswith(" " * INDENTATION_SPACES * indentation_level) and line.strip() not in ["{", "}"]:
                        logging.warning(f"Indentation issue at line {line_number}: Incorrect indentation.")
                        self.counts['indentation_check'] += 1

            logging.info(f"Indentation check completed - Count: {self.counts['indentation_check']}")
        except FileNotFoundError:
            logging.error(f"File not found: {self.script_path}")
        except Exception as e:
            logging.error(f"Error during indentation check: {str(e)}")

    class NamingConventionVisitor(c_ast.NodeVisitor):
        def __init__(self):
            self.module_prefix = None
            self.is_global_scope = True
            self.is_class_scope = False

        def visit_FileAST(self, node):
            if node.ext:
                for ext in node.ext:
                    if isinstance(ext, c_ast.Decl):
                        if ext.name and ext.name.startswith('MODULE_'):
                            self.module_prefix = ext.name
                self.generic_visit(node)

        def visit_FuncDef(self, node):
            if self.is_global_scope and not node.decl.name[0].islower():
                logging.warning(f"Function {node.decl.name} does not start with a lowercase letter")
                self.counts['naming_conventions_check'] += 1
            self.visit(node.body)

        def visit_Decl(self, node):
            if isinstance(node.type, c_ast.TypeDecl):
                if node.name.islower() and self.is_global_scope and not self.is_class_scope:
                    logging.warning(f"Global variable {node.name} does not start with 'g_'")
                    self.counts['naming_conventions_check'] += 1
                elif node.name.startswith('m_') and not self.is_class_scope:
                    logging.warning(f"Member {node.name} should not start with 'm_'")
                    self.counts['naming_conventions_check'] += 1
                elif node.name.upper() == node.name and self.is_global_scope:
                    logging.warning(f"Constant {node.name} should not be all uppercase")
                    self.counts['naming_conventions_check'] += 1
                elif self.module_prefix and not node.name.startswith(self.module_prefix):
                    logging.warning(f"Symbol {node.name} should have a prefix '{self.module_prefix}'")
                    self.counts['naming_conventions_check'] += 1
            elif isinstance(node.type, c_ast.PtrDecl):
                if not node.name.startswith('p_'):
                    logging.warning(f"Pointer variable {node.name} should start with 'p_'")
                    self.counts['naming_conventions_check'] += 1
            elif isinstance(node.type, c_ast.Struct):
                if not node.name[0].isupper() and self.is_global_scope:
                    logging.warning(f"Type/Class {node.name} does not start with an uppercase letter")
                    self.counts['naming_conventions_check'] += 1
                self.is_class_scope = True
                self.generic_visit(node)
                self.is_class_scope = False
            else:
                self.generic_visit(node)

        def visit_Compound(self, node):
            # Entering a function scope
            self.is_global_scope = False
            self.generic_visit(node)
            # Exiting a function scope
            self.is_global_scope = True

    def check_modularization(self):
        try:
            with open(self.script_path, "r") as script_file:
                lines = script_file.readlines()

            repeated_sequences = {}
            sequence_length = 3  # Minimum number of lines in a sequence to consider it for refactoring

            # Create sequences of lines
            for start_index in range(len(lines) - sequence_length + 1):
                sequence = tuple(lines[start_index:start_index + sequence_length])
                if all(len(line.strip()) > 1 for line in sequence):  # Check if all lines have more than one character
                    if sequence in repeated_sequences:
                        repeated_sequences[sequence].append(start_index + 1)  # Line numbers start from 1
                    else:
                        repeated_sequences[sequence] = [start_index + 1]

            # Determine the threshold for suggesting refactoring as a function
            repetition_threshold = 4

            for sequence, line_numbers in repeated_sequences.items():
                if len(line_numbers) >= repetition_threshold:
                    # Remove leading and trailing whitespace from the sequence for better readability in the log
                    formatted_sequence = ''.join(sequence).strip()
                    warning_message = f"Repetition detected: Sequence '{formatted_sequence}' repeated {len(line_numbers)} times. Consider refactoring as a function. Lines: {', '.join(map(str, line_numbers))}"
                    logging.warning(warning_message)
                    self.counts['modularization_check'] += 1

            logging.info(f"Modularization check completed - Count: {self.counts['modularization_check']}")

        except FileNotFoundError:
            logging.error(f"File not found: {self.script_path}")
        except Exception as e:
            logging.error(f"Error during modularization check: {str(e)}")

    def check_file_encoding(self):
        try:
            with open(self.script_path, 'rb') as f:
                encoding = ''
                for line in f:
                    try:
                        line.decode('utf-8')
                    except UnicodeDecodeError as e:
                        encoding = str(e)
                        logging.error(f"File is not UTF-8 encoded: {e}")
                        break
                if not encoding:
                    logging.info("File is UTF-8 encoded")
                    self.counts['file_encoding_check'] += 1

            logging.info(f"File encoding check completed - Count: {self.counts['file_encoding_check']}")

        except FileNotFoundError:
            logging.error(f"File not found: {self.script_path}")
        except Exception as e:
            logging.error(f"Error during file encoding check: {str(e)}")

    def check_consistency(self):
        try:
            with open(self.script_path, "r") as script_file:
                lines = script_file.readlines()

            # Check for consistent use of tabs or spaces for indentation
            indentation_type = None
            for line_number, line in enumerate(lines, start=1):
                leading_whitespace = len(line) - len(line.lstrip())
                if leading_whitespace:
                    if line[leading_whitespace] == '\t':
                        if indentation_type is None:
                            indentation_type = 'tabs'
                        elif indentation_type != 'tabs':
                            logging.warning(f"Inconsistent use of tabs and spaces for indentation at line {line_number}")
                            self.counts['consistency_check'] += 1
                    else:
                        if indentation_type is None:
                            indentation_type = 'spaces'
                        elif indentation_type != 'spaces':
                            logging.warning(f"Inconsistent use of tabs and spaces for indentation at line {line_number}")
                            self.counts['consistency_check'] += 1

            # Check for consistent line endings (CRLF or LF)
            line_endings = set()
            for line_number, line in enumerate(lines, start=1):
                if '\r\n' in line:
                    line_endings.add('CRLF')
                elif '\n' in line:
                    line_endings.add('LF')

            if len(line_endings) > 1:
                logging.warning("Inconsistent line endings found in the script. Use either CRLF or LF, not both.")
                self.counts['consistency_check'] += 1

            logging.info(f"Consistency check completed - Count: {self.counts['consistency_check']}")

        except FileNotFoundError:
            logging.error(f"File not found: {self.script_path}")
        except Exception as e:
            logging.error(f"Error during consistency check: {str(e)}")

    def check_code_reuse(self):
        try:
            with open(self.script_path, "r") as script_file:
                lines = script_file.readlines()

            function_definitions = []
            for line_number, line in enumerate(lines, start=1):
                if line.startswith("int ") or line.startswith("void "):
                    function_name = re.search(r'\b[a-zA-Z_][a-zA-Z0-9_]*\s*\(', line)
                    if function_name:
                        function_definitions.append((function_name.group().strip(), line_number))

            function_calls = {}
            for line_number, line in enumerate(lines, start=1):
                for function_name, _ in function_definitions:
                    if function_name in line:
                        if function_name in function_calls:
                            function_calls[function_name].append(line_number)
                        else:
                            function_calls[function_name] = [line_number]

            for function_name, call_line_numbers in function_calls.items():
                if len(call_line_numbers) > 1:
                    logging.warning(f"Function '{function_name}' called multiple times. Consider refactoring for code reuse. Call locations: {', '.join(map(str, call_line_numbers))}")
                    self.counts['code_reuse_check'] += 1

            logging.info(f"Code reuse check completed - Count: {self.counts['code_reuse_check']}")

        except FileNotFoundError:
            logging.error(f"File not found: {self.script_path}")
        except Exception as e:
            logging.error(f"Error during code reuse check: {str(e)}")

    def check_excess_whitespace(self):
        try:
            with open(self.script_path, "r") as script_file:
                lines = script_file.readlines()

            for line_number, line in enumerate(lines, start=1):
                stripped_line = line.strip()
                if stripped_line and re.search(r'\\s{2,}', stripped_line):
                    logging.warning(f"Excess whitespace detected: Line {line_number} '{stripped_line}' has more than one space between words.")
                    self.counts['excess_whitespace_check'] += 1

            logging.info(f"Excess whitespace check completed - Count: {self.counts['excess_whitespace_check']}")

        except FileNotFoundError:
            logging.error(f"File not found: {self.script_path}")
        except Exception as e:
            logging.error(f"Error during excess whitespace check: {str(e)}")

    def check_repetition(self):
        try:
            with open(self.script_path, "r") as script_file:
                lines = [line.strip() for line in script_file.readlines()]

            repeated_blocks = {}
            for i in range(len(lines) - 2):
                block_lines = lines[i:i+3]
                # Skip blocks that contain lines we want to omit
                if any(line.startswith(("//", "#if", "#else", "#endif", "return")) or len(line) == 1 for line in block_lines):
                    continue
                block = '\n'.join(block_lines)
                if block in repeated_blocks:
                    repeated_blocks[block].append(i + 1)
                else:
                    repeated_blocks[block] = [i + 1]

            for block_content, block_starts in repeated_blocks.items():
                if len(block_starts) > 1:
                    logging.warning(f"Repetition detected: Block starting with '{block_content.splitlines()[0]}' repeated {len(block_starts)} times. Consider refactoring by combining these lines into a function.")
                    self.counts['repetition_check'] += 1

            logging.info(f"Repetition check completed - Count: {self.counts['repetition_check']}")

        except FileNotFoundError:
            logging.error(f"File {self.script_path} not found.")

def send_email(sender_email, sender_password, recipient_email, attachment_path, counts):
    # Create a multipart message
    message = MIMEMultipart()   
    message['From'] = sender_email
    message['To'] = recipient_email

    # Get the current date and format it as desired
    current_date = datetime.now().strftime('%d-%m-%Y')
    subject = f"Script Analysis Log - {current_date}"
    message['Subject'] = subject

    # Add body to email
    body = "Please find attached the log file for the script analysis.\n\n"
    body += "Summary:\n\n"

    # Create a table for counts
    table = "<table style='border-collapse: collapse;'>"
    for check, count in counts.items():
        table += f"<tr><td style='border: 1px solid black; background-color: lightgrey;'>{check}</td><td style='border: 1px solid black; background-color: lightgrey;'>{count}</td></tr>"
    table += "</table>"

    body += table
    message.attach(MIMEText(body, 'html'))

    # Open the file to be sent
    filename = os.path.basename(attachment_path)
    attachment = open(attachment_path, "rb")

    # Add file as application/octet-stream
    part = MIMEBase("application", "octet-stream")
    part.set_payload(attachment.read())
    encoders.encode_base64(part)

    # Add header as key/value pair to attachment part
    part.add_header(
        "Content-Disposition",
        f"attachment; filename= {filename}",
    )

    # Add attachment to message and convert message to string
    message.attach(part)
    text = message.as_string()

    # Log into server and send email
    server = smtplib.SMTP(SMTP_SERVER, SMTP_PORT)
    server.starttls()
    server.login(sender_email, sender_password)
    server.sendmail(sender_email, recipient_email, text)
    server.quit()

    # Log the counts sent in the email body
    logging.info(f"Counts sent in email body: {counts}")

if __name__ == "__main__":
    analyzer = ScriptAnalyzer(script_path, recipient_email, sender_email, sender_password)
    analyzer.run_analysis()