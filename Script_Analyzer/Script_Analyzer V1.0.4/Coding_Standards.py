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
SENDER_EMAIL = 'vaishnavi.m@thinkpalm.com'
SENDER_PASSWORD = 'Malu$123'
RECIPIENT_EMAIL = 'manu.m@thinkpalm.com'
SMTP_SERVER = 'smtp-mail.outlook.com'
SMTP_PORT = 587

# Set global indentation and iteration values
INDENTATION_SPACES = 4
ITERATION_VALUES = {
    'MAX_FUNCTION_COUNT': 3,  # Maximum number of functions expected in the script
    'MAX_SUBROUTINE_COUNT': 3  # Maximum number of subroutines expected in the script
}

class ScriptAnalyzer:
    def __init__(self, script_path):
        self.script_path = Path(script_path)
        self.log_file = self.get_log_file_name()
        logging.basicConfig(filename=self.log_file, level=logging.INFO,
                            format='%(asctime)s - %(levelname)s - %(message)s')

    def get_log_file_name(self):
        current_datetime = datetime.now().strftime("%H-%M-%S-on-%d-%m-%Y")
        log_folder = self.script_path.parent / "Logs"
        log_folder.mkdir(parents=True, exist_ok=True)  # Create Logs folder if it doesn't exist
        log_file_name = f"Logs-{self.script_path.stem}-at-{current_datetime}.log"
        return log_folder / log_file_name

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

            # Check whitespace usage
            self.check_whitespace()
            
            # # Check memory leakages (for C scripts)
            # if str(self.script_path).endswith(".cpp"):
            #     self.check_memory_leaks()
            # else:
            #     print("Script Cannot be analyzed for Memry Leaks.")

            # Print summary of the analysis results
            print("Script Analysis completed.")
            # Creating Log with Analysis in Log Directory    
            logging.info("Script Analysis completed.")
            
            # Email the log file
            sender_email = SENDER_EMAIL
            sender_password = SENDER_PASSWORD
            recipient_email = RECIPIENT_EMAIL
            attachment_path = self.log_file
            send_email(sender_email, sender_password, recipient_email, attachment_path)

        except Exception as e:
            logging.error(f"Error during analysis: {str(e)}")

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
        except Exception as e:
            logging.error(f"Error during indentation check: {str(e)}")

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
                    self.visit(node.body)

                def visit_Decl(self, node):
                    if isinstance(node.type, c_ast.TypeDecl):
                        if node.name.islower():
                            logging.warning(f"Variable {node.name} does not start with an uppercase letter")
                        elif node.name.upper() == node.name:
                            logging.warning(f"Constant {node.name} should not be all uppercase")
                        elif self.module_prefix and not node.name.startswith(self.module_prefix):
                            logging.warning(f"Symbol {node.name} should have a prefix '{self.module_prefix}'")
                    elif isinstance(node.type, c_ast.PtrDecl):
                        if not node.name.startswith('p_'):
                            logging.warning(f"Pointer variable {node.name} should start with 'p_'")
                    elif isinstance(node.type, c_ast.Struct):
                        if not node.name[0].isupper():
                            logging.warning(f"Type/Class {node.name} does not start with an uppercase letter")
                    self.generic_visit(node)

            v = NamingConventionVisitor()
            v.visit(ast)

        except FileNotFoundError:
            logging.error(f"File not found: {self.script_path}")
        except Exception as e:
            logging.error(f"Error during naming conventions check: {str(e)}")

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
            with open(self.script_path, "r") as script_file:
                lines = script_file.readlines()

            num_functions = 0
            for line in lines:
                if re.match(r'^[a-zA-Z_][a-zA-Z0-9_]*\s*\(\s*\)\s*\{', line) and "main" not in line:
                    num_functions += 1

            if num_functions < ITERATION_VALUES['MAX_FUNCTION_COUNT']:
                logging.warning(f"Modularization issue: The file {self.script_path} should have at least {ITERATION_VALUES['MAX_FUNCTION_COUNT']} functions.")

        except FileNotFoundError:
            logging.error(f"File not found: {self.script_path}")
        except Exception as e:
            logging.error(f"Error during modularization check: {str(e)}")

    def check_file_encoding(self):
        try:
            with open(self.script_path, "rb") as script_file:
                script_file.read().decode('utf-8')
        except UnicodeDecodeError as e:
            logging.error(f"File encoding issue: {str(e)} at {self.script_path}")

    def check_consistency(self):
        try:
            with open(self.script_path, "r") as script_file:
                lines = script_file.readlines()

                for line_number, line in enumerate(lines[:-1], start=1):
                    if not line.endswith(('\n', '\r\n', '\r')):
                        logging.warning(f"Consistency issue at line {line_number}: Inconsistent line ending.")

                last_line = lines[-1].strip()
                if last_line != "}":
                    logging.warning(f"Consistency issue: Incorrect syntax at the end of the file. Expected '}}', found '{last_line}'.")

        except FileNotFoundError:
            logging.error(f"File not found: {self.script_path}")
        except Exception as e:
            logging.error(f"Error during consistency check: {str(e)}")

    def check_code_reuse(self):
        try:
            with open(self.script_path, "r") as script_file:
                lines = script_file.readlines()

                for line_number, line in enumerate(lines, start=1):
                    if re.match(r'^\s*(import|require)\s+', line):
                        logging.warning(f"Code reuse issue at line {line_number}: Possible code duplication.")
        except FileNotFoundError:
            logging.error(f"File not found: {self.script_path}")
        except Exception as e:
            logging.error(f"Error during code reuse check: {str(e)}")

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
                    if line.rstrip('\n').endswith(' '):
                        logging.warning(f"Whitespace issue at line {line_number}: Line ends with a space.")
        except FileNotFoundError:
            logging.error(f"File not found: {self.script_path}")
        except Exception as e:
            logging.error(f"Error during whitespace check: {str(e)}")
        
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
            except subprocess.CalledProcessError as e:
                logging.error(f"Error running memory leak check: {e}")
                return  # Exit the function early

        except FileNotFoundError:
            logging.error("Memory leak check tool not found. Please install the required tool.")
        except Exception as e:
            logging.error(f"Error checking memory leaks: {str(e)}")

def send_email(sender_email, sender_password, recipient_email, attachment_path):

    # Create a multipart message
    message = MIMEMultipart()
    message['From'] = sender_email
    message['To'] = recipient_email

    # Get the current date and format it as desired
    current_date = datetime.now().strftime('%Y-%m-%d')
    subject = f'Script Analyzer - {current_date}'  # Add current date to the subject
    message['Subject'] = subject

    # Add body text
    body = 'Please find the attached file.'
    message.attach(MIMEText(body, 'plain'))

    # Add attachment
    if attachment_path:
        attachment_filename = attachment_path.name
        attachment = open(attachment_path, "rb")
        part = MIMEBase('application', 'octet-stream')
        part.set_payload(attachment.read())
        encoders.encode_base64(part)
        part.add_header('Content-Disposition', f"attachment; filename= {attachment_filename}")
        message.attach(part)
        attachment.close()  # Close the file after reading

    # Connect to the SMTP server and send the email
    try:
        server = smtplib.SMTP(SMTP_SERVER, SMTP_PORT)
        server.starttls()
        server.login(sender_email, sender_password)
        server.sendmail(sender_email, recipient_email, message.as_string())
        server.quit()
        print("Email sent successfully!")

    except (smtplib.SMTPException, IOError, OSError, Exception) as e:
        if isinstance(e, smtplib.SMTPException):
            print(f"Failed to send email: {e}")
        elif isinstance(e, IOError):
            print(f"I/O error occurred: {e}")
        elif isinstance(e, OSError):
            print(f"OS error occurred: {e}")
        else:
            print(f"Unexpected error: {e}")
        logging.error(f"Error occurred while sending email: {e}")

if __name__ == "__main__":
    script_path = input('Enter the Full Path to the Script that needs to be Reviewed (<Filename>.cpp)): \n')
    analyzer = ScriptAnalyzer(script_path)
    analyzer.run_analysis()