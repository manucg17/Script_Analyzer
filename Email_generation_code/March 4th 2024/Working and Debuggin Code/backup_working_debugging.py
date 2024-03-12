import smtplib
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
from email.mime.application import MIMEApplication
import os

# Set working directory
file_path = 'D:/Python/Email_generation_code/'
os.chdir(file_path)

try:
    # Read the text file
    file_name = 'Testing_Email.txt'
    with open(file_name, 'r') as file:
        file_content = file.read()

    # Edit the file content
    edited_content = file_content + "\nEdited in VSCode by manu!"

    # Save the edited content back to the file
    with open(file_name, 'w') as file:
        file.write(edited_content)

    # Prepare the email
    sender_email = 'manu.m@thinkpalm.com'
    receiver_email_list = ['niyas.ns@thinkpalm.com']

    subject = 'Testing to send email with an Edited File -Revathi'
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

    # Send the email using port 25
    # smtp_server = 'thinkpalm-com.mail.protection.outlook.com'
    # smtp_port = 25
    # server = smtplib.SMTP(smtp_server, smtp_port)
    # server.starttls()
    # server.login(sender_email, os.environ.get('EMAIL_PASSWORD'))
    # server.sendmail(sender_email, receiver_email_list, message.as_string())
    # server.quit()
    
    smtp_server = 'smtp.office365.com'
    smtp_port = 587
    server = smtplib.SMTP(smtp_server, smtp_port)
    server.connect(smtp_server, smtp_port)
    server.ehlo()
    server.starttls()
    server.login(sender_email, os.environ.get('EMAIL_PASSWORD'))
    server.sendmail(sender_email, receiver_email_list, message.as_string())
    server.quit()

    print(f"Email sent successfully to {', '.join(receiver_email_list)}!")

except FileNotFoundError as e:
    print(f"File '{file_name}' not found: {e}")
except smtplib.SMTPException as e:
    print(f"An error occurred while sending email: {e}")
except Exception as e:
    print(f"Unexpected error: {e}")