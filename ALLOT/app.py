import os
import shutil
import logging
from flask import Flask, render_template, request, redirect, url_for, flash
from werkzeug.utils import secure_filename
from Script_Analyzer import ScriptAnalyzer

app = Flask(__name__)
app.secret_key = 'supersecretkey'
UPLOAD_FOLDER = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'Uploads')
ALLOWED_EXTENSIONS = {'cpp'}

# Set global configuration values
sender_email = 'manu.m@thinkpalm.com'
sender_password = 'civiC@3547'
SMTP_SERVER = 'smtp-mail.outlook.com'
SMTP_PORT = 587

# Delete the existing directory if it exists
if os.path.exists(UPLOAD_FOLDER):
    shutil.rmtree(UPLOAD_FOLDER)

# Create a new directory
os.makedirs(UPLOAD_FOLDER)

# Set the permission to 777
os.chmod(UPLOAD_FOLDER, 0o777)

@app.route('/')
def index():
    return render_template('index.html')

def allowed_file(filename):
    return '.' in filename and filename.rsplit('.', 1)[1].lower() in ALLOWED_EXTENSIONS

@app.route('/upload', methods=['POST'])
def upload_file():
    recipient_email = request.form.get('recipient_email')  # Retrieve recipient email from the form
    if 'file' not in request.files:
        flash('No file part')
        return redirect(request.url)
    file = request.files['file']
    if file.filename == '':
        flash('No selected file')
        return redirect(request.url)
    if file and allowed_file(file.filename):
        filename = secure_filename(file.filename)
        # Save the uploaded file to the uploads folder
        file_path = os.path.join(UPLOAD_FOLDER, filename)
        file.save(file_path)
        analyzer = ScriptAnalyzer(file_path, recipient_email, sender_email, sender_password)
        try:
            analyzer.run_analysis()
            # Remove all handlers from the logger
            for handler in logging.root.handlers[:]:
                logging.root.removeHandler(handler)
            flash('File successfully uploaded and analyzed. Email sent successfully')
        except Exception as e:
            flash(f'Error analyzing the file and sending email: {str(e)}', 'error')
        return redirect(url_for('index'))
    else:
        flash('Allowed file types are .cpp', 'error')
        return redirect(request.url)

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)