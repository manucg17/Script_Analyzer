import re
import logging
from pathlib import Path
from datetime import datetime
import smtplib
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText
from email.mime.base import MIMEBase
from email import encoders

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

            # Check performance considerations
            self.check_performance()

            # Check whitespace usage
            self.check_whitespace()

            # Check memory leakages (for C scripts)
            if str(self.script_path).endswith(".c"):
                self.check_memory_leaks()
            elif str(self.script_path).endswith(".pl"):
                self.check_perl_specific_checks()
            
            # Print summary of the analysis results
            print("Script analysis completed successfully.")
            # Creating Log with Analysis in Log Directory    
            logging.info("Script analysis completed successfully.")
            
            # Email the log file
            sender_email = 'vaishnavi.m@thinkpalm.com'
            sender_password = 'Malu$123'
            recipient_email = 'manu.m@thinkpalm.com'
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
                    # Skip empty lines and user comments
                    if not line.strip() or line.strip().startswith("//") or line.strip().startswith("/*"):
                        continue

                    # Check for TAB space usage
                    if "\t" in line:
                        logging.warning(f"Indentation issue at line {line_number}: TAB space used. Convert TABs to spaces.")

                    # Check #include lines
                    if line.strip().startswith("#include"):
                        # Check for correct syntax after #include
                        if not re.match(r'^#include\s+<\w+\.h>\s*$', line.strip()):
                            logging.warning(f"Syntax issue at line {line_number}: Incorrect syntax after #include.")
                        continue  # Skip further checks for #include lines

                    # Check for function definitions
                    if "{" in line and "(" in line and not inside_function:
                        if line.strip().endswith("{"):
                            inside_function = True
                            indentation_level += 1
                            continue  # Skip further checks for function definition lines

                    if inside_function:
                        # Check control structures indentation
                        control_structures = ["for", "while", "do", "if", "else", "elif", "elseif", "unless"]
                        for control_structure in control_structures:
                            if control_structure in line.strip():
                                if not line.startswith(" " * 4 * indentation_level):
                                    logging.warning(f"Indentation issue at line {line_number}: Incorrect indentation for {control_structure} statement.")
                                if line.strip().endswith("{"):
                                    indentation_level += 1
                            if "}" in line:
                                indentation_level -= 1
                                if indentation_level == 0:
                                    inside_function = False
                                    continue

                    # Check indentation of non-empty, non-comment lines
                    if not line.startswith(" " * 4 * indentation_level):
                        logging.warning(f"Indentation issue at line {line_number}: Incorrect indentation.")

        except FileNotFoundError:
            logging.error(f"File not found: {self.script_path}")
        except Exception as e:
            logging.error(f"Error during indentation check: {str(e)}")


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
            logging.error(f"File not found: {self.script_path}")
        except Exception as e:
            logging.error(f"Error during naming conventions check: {str(e)}")


    def check_common_naming_conventions(self, line, line_number):
        # Implement common naming conventions checks
        # Example: Check if variable names follow a consistent pattern
        if str(self.script_path).endswith(".c"):
            # Check for C-specific naming conventions
            if re.match(r'^[a-zA-Z_][a-zA-Z0-9_]*$', line):
                logging.warning(f"Naming convention issue at line {line_number}: {line}")
        elif str(self.script_path).endswith(".pl"):
            # Check for Perl-specific naming conventions
            if re.match(r'^[a-zA-Z][a-zA-Z0-9]*$', line):
                logging.warning(f"Naming convention issue at line {line_number}: {line}")


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

                # if num_functions < 3:
                #     logging.warning(f"Modularization issue: The file {self.script_path} should have at least 3 functions.")

            elif str(self.script_path).endswith(".pl"):
                num_subroutines = 0
                for line in lines:
                    if re.match(r'^\s*sub\s', line):
                        num_subroutines += 1

                # if num_subroutines < 3:
                #     logging.warning(f"Modularization issue: The file {self.script_path} should have at least 3 subroutines.")

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

                # Check for consistent line endings (e.g., only '\n', '\r\n', or '\r')
                for line_number, line in enumerate(lines[:-1], start=1):
                    if not line.endswith(('\n', '\r\n', '\r')):
                        logging.warning(f"Consistency issue at line {line_number}: Inconsistent line ending.")

                # Check the last line for correct syntax
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
                    # Check for code reuse patterns
                    if re.match(r'^\s*(import|require)\s+', line):
                        logging.warning(f"Code reuse issue at line {line_number}: Possible code duplication.")
        except FileNotFoundError:
            logging.error(f"File not found: {self.script_path}")
        except Exception as e:
            logging.error(f"Error during code reuse check: {str(e)}")


    def check_performance(self):
        try:
            # Placeholder for performance checks
            pass
        except Exception as e:
            logging.error(f"Error during performance check: {str(e)}")


    def check_whitespace(self):
        try:
            with open(self.script_path, "r") as script_file:
                lines = script_file.readlines()

                for line_number, line in enumerate(lines, start=1):
                    # Check for excessive whitespace within lines
                    if not line.strip().startswith("#include"):
                        stripped_line = line.strip()  # strip leading and trailing spaces
                        if '  ' in stripped_line:  # check for two or more consecutive spaces
                            logging.warning(f"Whitespace issue at line {line_number}: Excessive whitespace within line.")
                        if line.rstrip('\n').endswith(' '):  # check if line ends with a space
                            logging.warning(f"Whitespace issue at line {line_number}: Line ends with a space.")
        except FileNotFoundError:
            logging.error(f"File not found: {self.script_path}")
        except Exception as e:
            logging.error(f"Error during whitespace check: {str(e)}")


    def check_memory_leaks(self):
        try:
            # Placeholder for memory leak checks
            pass
        except Exception as e:
            logging.error(f"Error during memory leak check: {str(e)}")


    def check_perl_specific_checks(self):
        try:
            # Placeholder for Perl-specific checks
            pass
        except Exception as e:
            logging.error(f"Error during Perl-specific check: {str(e)}")


def send_email(sender_email, sender_password, recipient_email, attachment_path):
    smtp_server = 'smtp-mail.outlook.com'
    smtp_port = 587

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
        server = smtplib.SMTP(smtp_server, smtp_port)
        server.starttls()
        server.login(sender_email, sender_password)
        server.sendmail(sender_email, recipient_email, message.as_string())
        server.quit()
        print("Email sent successfully!")
        
    except smtplib.SMTPException as e:
        print(f"Failed to send email: {e}")
    except Exception as e:
        print(f"Unexpected error: {e}")

if __name__ == "__main__":
    script_path = input("Enter the Full Path to the Access the Script that needs to be Reviewed (Supports -> C (*.c) CPP (*.cpp) or Perl (*.pl or *.pm)): ")
    analyzer = ScriptAnalyzer(script_path)
    analyzer.run_analysis()