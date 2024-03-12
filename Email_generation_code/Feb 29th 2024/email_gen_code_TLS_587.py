### Sending Email using  SMTP in Python 
### SMTP Server Used: thinkpalm-com.mail.protection.outlook.com'
### SMTP Port Used: 587 -- TLS
 
import smtplib
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart

try:
    # Read the text file
    with open(r'D:\Python\Email_generation_code\Testing_Email_TLS.txt', 'r') as file:
        file_content = file.read()

    # Edit the file content (for example, add some text)
    edited_content = file_content + "\nEdited in VSCode by manu!"

    # Save the edited content back to the file
    with open(r'D:\Python\Email_generation_code\Testing_Email_TLS.txt', 'w') as file:
        file.write(edited_content)

    # Prepare the email
    sender_email = 'manu.m@thinkpalm.com'
    receiver_email_list = 'vaishnavi.m@thinkpalm.com, revathi.b@thinkpalm.com, sharoon.j@thinkpalm.com'
    subject = 'Testing to send email with an Edited File'
    body = 'Please find the edited file attached.'
    message = MIMEMultipart()
    message['From'] = sender_email
    message['To'] = receiver_email_list
    message['Subject'] = subject
    message.attach(MIMEText(body, 'plain'))

    # Attach the edited file to the email
    attachment = MIMEText(edited_content)
    attachment.add_header('Content-Disposition', 'attachment', filename=r'D:\Python\Email_generation_code\Testing_Email_TLS.txt')
    message.attach(attachment)

    # Send the email using port 587 with authentication
    with smtplib.SMTP('thinkpalm-com.mail.protection.outlook.com', 587) as server:
        server.starttls()  # Enable encryption
        server.login('manu.m@thinkpalm.com', 'civiC@3547')  # Use your email address and password for authentication
        server.sendmail(sender_email, receiver_email_list, message.as_string())

    print("Email sent successfully!")

except Exception as e:
    print(f"An error occurred: {e}")