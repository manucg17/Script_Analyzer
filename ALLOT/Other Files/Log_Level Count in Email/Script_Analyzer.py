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
from tabulate import tabulate

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
        self.log_counts = {
            'Fatal': 0,
            'Error': 0,
            'Warning': 0,
            'Info': 0,
            'Debug': 0,
            'Trace': 0
        }
        logging.basicConfig(filename=self.log_file, level=logging.INFO,
                            format='%(asctime)s - %(levelname)s - %(message)s')

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
                    self.log_counts['Error'] += 1  # Increment error count
        except FileNotFoundError:
            logging.error(f"File not found: {self.script_path}")
            self.log_counts['Error'] += 1  # Increment error count
        except Exception as e:
            logging.error(f"Error during include directive check: {str(e)}")
            self.log_counts['Error'] += 1  # Increment error count

    def run_analysis(self):
        try:
            # Check for mandatory #include directive
            self.check_include_directive()
                            
            # Check script indentation
            self.check_total_lines()

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

            # Check whitespace usage
            self.check_whitespace()

            # Check memory leaks
            self.check_memory_leaks()

            # Print summary of the analysis results
            print("Script Analysis completed.")
            # Creating Log with Analysis in Log Directory
            logging.info("Script Analysis completed.")

            # Email the log file
            sender_email = self.sender_email
            sender_password = self.sender_password
            recipient_email = self.recipient_email
            attachment_path = self.log_file
            send_email(sender_email, sender_password, recipient_email, attachment_path, self.log_counts)

        except Exception as e:
            logging.error(f"Error during analysis: {str(e)}")

    def check_total_lines(self):
        try:
            with open(self.script_path, "r") as script_file:
                lines = script_file.readlines()
                total_lines = len(lines)
                if total_lines > EXPECTED_LINE_COUNT:
                    logging.warning(f"Total number of lines ({total_lines}) exceeds the recommended maximum of 2000 lines.")
                    self.log_counts['Warning'] += 1  # Increment warning count
        except FileNotFoundError:
            logging.error(f"File not found: {self.script_path}")
            self.log_counts['Error'] += 1  # Increment error count
        except Exception as e:
            logging.error(f"Error during total lines check: {str(e)}")
            self.log_counts['Error'] += 1  # Increment error count

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

                    if line.strip().startswith("#include"):
                        if not re.match(r'^#include\s+(<\S+\.h>|"\S+\.h")\s*$', line.strip()):
                            logging.warning(f"Syntax issue at line {line_number}: Incorrect syntax after #include.")
                        continue

                    if "{" in line and "(" in line and not inside_function:
                        if line.strip().endswith("{"):
                            inside_function = True
                            indentation_level += 1
                            continue

                    if inside_function:
                        control_structures = ["for", "while", "do", "if", "else", "elif", "elseif", "unless"]
                        for control_structure in control_structures:
                            if control_structure in line.strip():
                                if not line.startswith(" " * INDENTATION_SPACES * indentation_level):
                                    logging.warning(f"Indentation issue at line {line_number}: Incorrect indentation for {control_structure} statement.")
                                if line.strip().endswith("{"):
                                    indentation_level += 1
                            if "}" in line:
                                indentation_level -= 1
                                if indentation_level == 0:
                                    inside_function = False
                                    continue

                    if not line.startswith(" " * INDENTATION_SPACES * indentation_level):
                        logging.warning(f"Indentation issue at line {line_number}: Incorrect indentation.")

        except FileNotFoundError:
            logging.error(f"File not found: {self.script_path}")
            self.log_counts['Error'] += 1  # Increment error count
        except Exception as e:
            logging.error(f"Error during indentation check: {str(e)}")
            self.log_counts['Error'] += 1  # Increment error count

    def check_naming_conventions(self):
        try:
            processed_script = self.preprocess_cpp_file()
            ast = parse_file(processed_script)

            class NamingConventionVisitor(c_ast.NodeVisitor):
                def __init__(self):
                    self.module_prefix = None

                def visit_FileAST(self, node):
                    if node.ext:
                        for ext in node.ext:
                            if isinstance(ext, c_ast.Decl):
                                if ext.name and ext.name.startswith('MODULE_'):
                                    self.module_prefix = ext.name
                        self.generic_visit(node)

                def visit_FuncDef(self, node):
                    if not node.decl.name[0].islower():
                        logging.warning(f"Function {node.decl.name} does not start with a lowercase letter")
                        self.log_counts['Warning'] += 1  # Increment warning count
                    self.generic_visit(node)

                def visit_Decl(self, node):
                    if isinstance(node.type, c_ast.TypeDecl):
                        if node.name.islower():
                            logging.warning(f"Variable {node.name} does not start with an uppercase letter")
                            self.log_counts['Warning'] += 1  # Increment warning count
                        elif node.name.upper() == node.name:
                            logging.warning(f"Constant {node.name} should not be all uppercase")
                            self.log_counts['Warning'] += 1  # Increment warning count
                        elif self.module_prefix and not node.name.startswith(self.module_prefix):
                            logging.warning(f"Symbol {node.name} should have a prefix '{self.module_prefix}'")
                            self.log_counts['Warning'] += 1  # Increment warning count
                    elif isinstance(node.type, c_ast.PtrDecl):
                        if not node.name.startswith('p_'):
                            logging.warning(f"Pointer variable {node.name} should start with 'p_'")
                            self.log_counts['Warning'] += 1  # Increment warning count
                    elif isinstance(node.type, c_ast.FuncDecl):
                        if not node.name.startswith('fn_'):
                            logging.warning(f"Function pointer {node.name} does not start with 'fn_' prefix")
                            self.log_counts['Warning'] += 1  # Increment warning count

                    self.generic_visit(node)

            visitor = NamingConventionVisitor()
            visitor.visit(ast)

        except Exception as e:
            logging.error(f"Error during naming conventions check: {str(e)}")
            self.log_counts['Error'] += 1  # Increment error count

    def preprocess_cpp_file(self):
        processed_file_path = self.script_path.parent / f"{self.script_path.stem}_processed.cpp"
        try:
            command = ['cpp', '-o', str(processed_file_path), str(self.script_path)]
            result = subprocess.run(command, check=True, capture_output=True, text=True)
        except subprocess.CalledProcessError as e:
            logging.error(f"Error during preprocessing: {e}")
            if e.stderr:
                logging.error(f"cpp error: {e.stderr}")
        except Exception as e:
            logging.error(f"Error during preprocessing: {str(e)}")

        return processed_file_path

    def check_modularization(self):
        try:
            processed_script = self.preprocess_cpp_file()
            ast = parse_file(processed_script)

            class ModularizationVisitor(c_ast.NodeVisitor):
                def __init__(self):
                    self.function_count = 0
                    self.subroutine_count = 0

                def visit_FuncDef(self, node):
                    self.function_count += 1
                    self.generic_visit(node)

                def visit_Decl(self, node):
                    if isinstance(node.type, c_ast.FuncDecl):
                        self.subroutine_count += 1
                    self.generic_visit(node)

            visitor = ModularizationVisitor()
            visitor.visit(ast)

            if visitor.function_count > ITERATION_VALUES['MAX_FUNCTION_COUNT']:
                logging.warning(f"Exceeded maximum number of functions ({visitor.function_count} > {ITERATION_VALUES['MAX_FUNCTION_COUNT']})")
                self.log_counts['Warning'] += 1  # Increment warning count

            if visitor.subroutine_count > ITERATION_VALUES['MAX_SUBROUTINE_COUNT']:
                logging.warning(f"Exceeded maximum number of subroutines ({visitor.subroutine_count} > {ITERATION_VALUES['MAX_SUBROUTINE_COUNT']})")
                self.log_counts['Warning'] += 1  # Increment warning count

        except Exception as e:
            logging.error(f"Error during modularization check: {str(e)}")
            self.log_counts['Error'] += 1  # Increment error count

    def check_file_encoding(self):
        try:
            with open(self.script_path, "rb") as script_file:
                raw_data = script_file.read()
                encoding = 'utf-8'
                try:
                    raw_data.decode(encoding)
                except UnicodeDecodeError:
                    logging.warning(f"File is not encoded in UTF-8")
                    self.log_counts['Warning'] += 1  # Increment warning count

        except FileNotFoundError:
            logging.error(f"File not found: {self.script_path}")
            self.log_counts['Error'] += 1  # Increment error count
        except Exception as e:
            logging.error(f"Error during file encoding check: {str(e)}")
            self.log_counts['Error'] += 1  # Increment error count

    def check_consistency(self):
        try:
            with open(self.script_path, "r") as script_file:
                lines = script_file.readlines()

                for line_number, line in enumerate(lines[:-1], start=1):
                    if not line.endswith(('\n', '\r\n', '\r')):
                        logging.warning(f"Consistency issue at line {line_number}: Inconsistent line ending.")
                        self.log_counts['Warning'] += 1  # Increment warning count

                last_line = lines[-1].strip()
                if last_line != "}":
                    logging.warning(f"Consistency issue: Incorrect syntax at the end of the file. Expected '}}', found '{last_line}'.")
                    self.log_counts['Warning'] += 1  # Increment warning count

        except FileNotFoundError:
            logging.error(f"File not found: {self.script_path}")
            self.log_counts['Error'] += 1  # Increment error count
        except Exception as e:
            logging.error(f"Error during consistency check: {str(e)}")
            self.log_counts['Error'] += 1  # Increment error count

    def check_code_reuse(self):
        try:
            with open(self.script_path, "r") as script_file:
                lines = script_file.readlines()
    
                for line_number, line in enumerate(lines, start=1):
                    if re.match(r'^\s*(import|require)\s+', line):
                        logging.warning(f"Code reuse issue at line {line_number}: Possible code duplication.")
                        self.log_counts['Warning'] += 1  # Increment warning count
        except FileNotFoundError:
            logging.error(f"File not found: {self.script_path}")
            self.log_counts['Error'] += 1  # Increment error count
        except Exception as e:
            logging.error(f"Error during code reuse check: {str(e)}")
            self.log_counts['Error'] += 1  # Increment error count
    
    def check_whitespace(self):
        try:
            with open(self.script_path, "r") as script_file:
                lines = script_file.readlines()
    
                for line_number, line in enumerate(lines, start=1):
                    if not line.strip() or line.strip().startswith("//") or line.strip().startswith("/*"):
                        continue
    
                    stripped_line = line.strip()
                    if '  ' in stripped_line:
                        logging.warning(f"Whitespace issue at line {line_number}: Excessive whitespace within line.")
                        self.log_counts['Warning'] += 1  # Increment warning count
                    if line.rstrip('\n').endswith(' '):
                        logging.warning(f"Whitespace issue at line {line_number}: Line ends with a space.")
                        self.log_counts['Warning'] += 1  # Increment warning count
        except FileNotFoundError:
            logging.error(f"File not found: {self.script_path}")
            self.log_counts['Error'] += 1  # Increment error count
        except Exception as e:
            logging.error(f"Error during whitespace check: {str(e)}")
            self.log_counts['Error'] += 1  # Increment error count
    
    def check_memory_leaks(self):
        try:
            cpp_file = str(self.script_path)
            executable_name = cpp_file.replace('.cpp', '.exe') if platform.system() == 'Windows' else cpp_file.replace('.cpp', '')
    
            try:
                if platform.system() == 'Windows':
                    result = subprocess.run(['g++', cpp_file, '-o', executable_name], check=True, capture_output=True, text=True)
                    if result.stderr:
                        logging.error(f"g++ error: {result.stderr}")
                    subprocess.check_call(['drmemory', '-logdir', 'logs', executable_name])
                else:
                    result = subprocess.run(['g++', cpp_file, '-o', executable_name], check=True, capture_output=True, text=True)
                    if result.stderr:
                        logging.error(f"g++ error: {result.stderr}")
                    valgrind_output = subprocess.check_output(['valgrind', '--leak-check=full', executable_name])
                    if 'no leaks are possible' not in valgrind_output.decode('utf-8'):
                        logging.warning("Memory leak detected!")
                        self.log_counts['Warning'] += 1  # Increment warning count
            except subprocess.CalledProcessError as e:
                logging.error(f"Error running memory leak check: {e}")
                return  # Exit the function early
    
        except FileNotFoundError:
            logging.error("Memory leak check tool not found. Please install the required tool.")
            self.log_counts['Error'] += 1  # Increment error count
        except Exception as e:
            logging.error(f"Error checking memory leaks: {str(e)}")
            self.log_counts['Error'] += 1  # Increment error count

def send_email(sender_email, sender_password, recipient_email, attachment_path, log_counts):
    try:
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
        body += "Summary of log counts:\n"
        table_data = [[level, count] for level, count in log_counts.items()]
        table = tabulate(table_data, headers=["Log Level", "Count"], tablefmt="html")
        message.attach(MIMEText(body + table, 'html'))

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

        logging.info("Email sent successfully.")
    except smtplib.SMTPAuthenticationError:
        logging.error("SMTP authentication error. Failed to send email.")
    except FileNotFoundError as e:
        logging.error(f"Attachment file not found: {attachment_path}")
    except Exception as e:
        logging.error(f"Error sending email: {e}")

if __name__ == "__main__":
    analyzer = ScriptAnalyzer(script_path, recipient_email, sender_email, sender_password)  # Use the uploaded file path and recipient email from the form
    analyzer.run_analysis()
