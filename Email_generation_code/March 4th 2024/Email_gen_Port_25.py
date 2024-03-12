import smtplib
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
from email.mime.application import MIMEApplication
import os

# Configuration
file_path = 'D:/Python/Email_generation_code/'
file_name = 'Testing_Email.txt'
sender_email = 'vaishnavi.m@thinkpalm.com'
receiver_email_list = ['manu.m@thinkpalm.com']
smtp_server = 'thinkpalm-com.mail.protection.outlook.com'
smtp_port = 25

# Read the text file
try:
    with open(os.path.join(file_path, file_name), 'r') as file:
        file_content = file.read()
except FileNotFoundError as e:
    print(f"File '{file_name}' not found: {e}")
    file_content = ''

# Edit the file content
edited_content = file_content + "\nEdited in VSCode by manu!"

# Save the edited content back to the file
with open(os.path.join(file_path, file_name), 'w') as file:
    file.write(edited_content)

# Prepare the email
subject = 'Testing to send email with an Edited File'
body = 'Please find the edited file attached.'
message = MIMEMultipart()
message['From'] = sender_email
message['To'] = ', '.join(receiver_email_list)
message['Subject'] = subject
message.attach(MIMEText(body, 'plain'))

# Attach the edited file to the email
attachment = MIMEApplication(edited_content.encode('utf-8'), _subtype='txt')
attachment.add_header('Content-Disposition', 'attachment', filename=file_name)
message.attach(attachment)

# Send the email using port 25 without SSL/TLS encryption and authentication
server = smtplib.SMTP(smtp_server, smtp_port)
server.sendmail(sender_email, receiver_email_list, message.as_string())
server.quit()

print(f"Email sent successfully to {', '.join(receiver_email_list)}!")