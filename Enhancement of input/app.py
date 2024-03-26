import os
from flask import Flask, render_template, request, redirect, url_for, flash
from werkzeug.utils import secure_filename
from Script_Analyzer import ScriptAnalyzer, send_email

app = Flask(__name__)
app.secret_key = 'supersecretkey'
UPLOAD_FOLDER = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'uploads')
ALLOWED_EXTENSIONS = {'cpp'}

# Set global configuration values
sender_email = 'manu.m@thinkpalm.com'
sender_password = 'civiC@3547'
SMTP_SERVER = 'smtp-mail.outlook.com'
SMTP_PORT = 587

os.makedirs(UPLOAD_FOLDER, exist_ok=True)

@app.route('/')
def index():
    return render_template('index.html')

def allowed_file(filename):
    return '.' in filename and \
           filename.rsplit('.', 1)[1].lower() in ALLOWED_EXTENSIONS

@app.route('/upload', methods=['POST'])
def upload_file():
    recipient_email = request.form['recipient_email']  # Retrieve recipient email from the form
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
        # Instantiate ScriptAnalyzer with the uploaded file path and recipient email
        analyzer = ScriptAnalyzer(file_path, recipient_email, sender_email, sender_password)
        analyzer.run_analysis()
        flash('File successfully uploaded and analyzed')
        return redirect(url_for('index'))
    else:
        flash('Allowed file types are .cpp')
        return redirect(request.url)

if __name__ == '__main__':
    app.run(host='192.168.0.192', port=5000, debug=True)
