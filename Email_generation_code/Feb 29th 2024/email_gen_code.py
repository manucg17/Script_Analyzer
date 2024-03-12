### Sending Email using  SMTP in Python 
### SMTP Server Used: thinkpalm-com.mail.protection.outlook.com'
### SMTP Port Used: 25

import smtplib
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart

try:
    # Read the text file
    with open(r'D:\Python\Email_generation_code\Testing_Email.txt', 'r') as file:
        file_content = file.read()

    # Edit the file content (for example, add some text)
    edited_content = file_content + "\nEdited in VSCode by manu!"

    # Save the edited content back to the file
    with open(r'D:\Python\Email_generation_code\Testing_Email.txt', 'w') as file:
        file.write(edited_content)

    # Prepare the email
    # Niyas N S <niyas.ns@thinkpalm.com>; Midhun Madhusoodanan <midhun.m@thinkpalm.com>
    
    sender_email = 'midhun.m@thinkpalm.com'
    receiver_email_list = 'niyas.ns@thinkpalm.com'
    
    # sender_email = 'niyas.ns@thinkpalm.com'
    # receiver_email_list = 'midhun.m@thinkpalm.com'
    # sender_email = 'vaishnavi.m@thinkpalm.com'
    # sender_email = 'manu.m@thinkpalm.com'
    # receiver_email_list = 'vaishnavi.m@thinkpalm.com, revathi.b@thinkpalm.com, sharoon.j@thinkpalm.com'
    # receiver_email_list = 'ananthalekshmi.g@thinkpalm.com'
    # receiver_email_list = 'manu.m@thinkpalm.com'
    subject = 'Testing to send email with an Edited File -Revathi'
    body = 'Please find the edited file attached.'
    message = MIMEMultipart()
    message['From'] = sender_email
    message['To'] = receiver_email_list
    message['Subject'] = subject
    message.attach(MIMEText(body, 'plain'))

    # Attach the edited file to the email
    attachment = MIMEText(edited_content)
    attachment.add_header('Content-Disposition', 'attachment', filename=r'D:\Python\Email_generation_code\Testing_Email.txt')
    message.attach(attachment)

    # Send the email using port 25
    with smtplib.SMTP('thinkpalm-com.mail.protection.outlook.com', 25) as server:
        server.sendmail(sender_email, receiver_email_list, message.as_string())

    print("Email sent successfully!")

except Exception as e:
    print(f"An error occurred: {e}")