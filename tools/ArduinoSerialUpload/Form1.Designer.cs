namespace ArduinoSerialUpload
{
    partial class Form1
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.components = new System.ComponentModel.Container();
            this.serialPort1 = new System.IO.Ports.SerialPort(this.components);
            this.txtSendString = new System.Windows.Forms.TextBox();
            this.btnSend = new System.Windows.Forms.Button();
            this.txtLog = new System.Windows.Forms.TextBox();
            this.btnReset = new System.Windows.Forms.Button();
            this.txtFilename = new System.Windows.Forms.TextBox();
            this.btnSendOne = new System.Windows.Forms.Button();
            this.openFileDialog1 = new System.Windows.Forms.OpenFileDialog();
            this.btnBrowseOne = new System.Windows.Forms.Button();
            this.txtDirectory = new System.Windows.Forms.TextBox();
            this.btnSendDir = new System.Windows.Forms.Button();
            this.lstWorkQueue = new System.Windows.Forms.ListBox();
            this.edtSerialPort = new System.Windows.Forms.TextBox();
            this.label1 = new System.Windows.Forms.Label();
            this.chkComOpen = new System.Windows.Forms.CheckBox();
            this.SuspendLayout();
            // 
            // serialPort1
            // 
            this.serialPort1.BaudRate = 57600;
            this.serialPort1.PortName = "COM3";
            this.serialPort1.DataReceived += new System.IO.Ports.SerialDataReceivedEventHandler(this.serialPort1_DataReceived);
            // 
            // txtSendString
            // 
            this.txtSendString.Location = new System.Drawing.Point(12, 12);
            this.txtSendString.Name = "txtSendString";
            this.txtSendString.Size = new System.Drawing.Size(179, 20);
            this.txtSendString.TabIndex = 0;
            this.txtSendString.Text = "START 0 1024";
            // 
            // btnSend
            // 
            this.btnSend.Location = new System.Drawing.Point(197, 12);
            this.btnSend.Name = "btnSend";
            this.btnSend.Size = new System.Drawing.Size(75, 23);
            this.btnSend.TabIndex = 1;
            this.btnSend.Text = "Send";
            this.btnSend.UseVisualStyleBackColor = true;
            this.btnSend.Click += new System.EventHandler(this.btnSend_Click);
            // 
            // txtLog
            // 
            this.txtLog.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom)
                        | System.Windows.Forms.AnchorStyles.Left)
                        | System.Windows.Forms.AnchorStyles.Right)));
            this.txtLog.Font = new System.Drawing.Font("Courier New", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.txtLog.Location = new System.Drawing.Point(12, 91);
            this.txtLog.Multiline = true;
            this.txtLog.Name = "txtLog";
            this.txtLog.Size = new System.Drawing.Size(578, 300);
            this.txtLog.TabIndex = 2;
            // 
            // btnReset
            // 
            this.btnReset.Location = new System.Drawing.Point(515, 51);
            this.btnReset.Name = "btnReset";
            this.btnReset.Size = new System.Drawing.Size(75, 23);
            this.btnReset.TabIndex = 3;
            this.btnReset.Text = "Reset";
            this.btnReset.UseVisualStyleBackColor = true;
            this.btnReset.Click += new System.EventHandler(this.btnReset_Click);
            // 
            // txtFilename
            // 
            this.txtFilename.Location = new System.Drawing.Point(12, 38);
            this.txtFilename.Name = "txtFilename";
            this.txtFilename.Size = new System.Drawing.Size(179, 20);
            this.txtFilename.TabIndex = 4;
            this.txtFilename.Text = "c:\\arduino\\www\\send\\index.html";
            // 
            // btnSendOne
            // 
            this.btnSendOne.Location = new System.Drawing.Point(197, 36);
            this.btnSendOne.Name = "btnSendOne";
            this.btnSendOne.Size = new System.Drawing.Size(75, 23);
            this.btnSendOne.TabIndex = 5;
            this.btnSendOne.Text = "SendFile";
            this.btnSendOne.UseVisualStyleBackColor = true;
            this.btnSendOne.Click += new System.EventHandler(this.btnSendOne_Click);
            // 
            // openFileDialog1
            // 
            this.openFileDialog1.FileName = "openFileDialog1";
            // 
            // btnBrowseOne
            // 
            this.btnBrowseOne.Location = new System.Drawing.Point(273, 36);
            this.btnBrowseOne.Name = "btnBrowseOne";
            this.btnBrowseOne.Size = new System.Drawing.Size(27, 23);
            this.btnBrowseOne.TabIndex = 6;
            this.btnBrowseOne.Text = "...";
            this.btnBrowseOne.UseVisualStyleBackColor = true;
            this.btnBrowseOne.Click += new System.EventHandler(this.btnBrowseOne_Click);
            // 
            // txtDirectory
            // 
            this.txtDirectory.Location = new System.Drawing.Point(12, 64);
            this.txtDirectory.Name = "txtDirectory";
            this.txtDirectory.Size = new System.Drawing.Size(179, 20);
            this.txtDirectory.TabIndex = 7;
            this.txtDirectory.Text = "C:\\Arduino\\www\\send\\";
            // 
            // btnSendDir
            // 
            this.btnSendDir.Location = new System.Drawing.Point(197, 61);
            this.btnSendDir.Name = "btnSendDir";
            this.btnSendDir.Size = new System.Drawing.Size(75, 23);
            this.btnSendDir.TabIndex = 8;
            this.btnSendDir.Text = "SendDir";
            this.btnSendDir.UseVisualStyleBackColor = true;
            this.btnSendDir.Click += new System.EventHandler(this.btnSendDir_Click);
            // 
            // lstWorkQueue
            // 
            this.lstWorkQueue.FormattingEnabled = true;
            this.lstWorkQueue.Location = new System.Drawing.Point(306, 9);
            this.lstWorkQueue.Name = "lstWorkQueue";
            this.lstWorkQueue.Size = new System.Drawing.Size(120, 69);
            this.lstWorkQueue.TabIndex = 9;
            // 
            // edtSerialPort
            // 
            this.edtSerialPort.Location = new System.Drawing.Point(571, 9);
            this.edtSerialPort.Name = "edtSerialPort";
            this.edtSerialPort.Size = new System.Drawing.Size(19, 20);
            this.edtSerialPort.TabIndex = 10;
            this.edtSerialPort.Text = "3";
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(512, 12);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(53, 13);
            this.label1.TabIndex = 11;
            this.label1.Text = "COM Port";
            // 
            // chkComOpen
            // 
            this.chkComOpen.AutoSize = true;
            this.chkComOpen.Location = new System.Drawing.Point(515, 28);
            this.chkComOpen.Name = "chkComOpen";
            this.chkComOpen.Size = new System.Drawing.Size(52, 17);
            this.chkComOpen.TabIndex = 12;
            this.chkComOpen.Text = "Open";
            this.chkComOpen.UseVisualStyleBackColor = true;
            this.chkComOpen.CheckedChanged += new System.EventHandler(this.chkComOpen_CheckedChanged);
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(602, 403);
            this.Controls.Add(this.chkComOpen);
            this.Controls.Add(this.label1);
            this.Controls.Add(this.edtSerialPort);
            this.Controls.Add(this.lstWorkQueue);
            this.Controls.Add(this.btnSendDir);
            this.Controls.Add(this.txtDirectory);
            this.Controls.Add(this.btnBrowseOne);
            this.Controls.Add(this.btnSendOne);
            this.Controls.Add(this.txtFilename);
            this.Controls.Add(this.btnReset);
            this.Controls.Add(this.txtLog);
            this.Controls.Add(this.btnSend);
            this.Controls.Add(this.txtSendString);
            this.Name = "Form1";
            this.Text = "Form1";
            this.Load += new System.EventHandler(this.Form1_Load);
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.IO.Ports.SerialPort serialPort1;
        private System.Windows.Forms.TextBox txtSendString;
        private System.Windows.Forms.Button btnSend;
        private System.Windows.Forms.TextBox txtLog;
        private System.Windows.Forms.Button btnReset;
        private System.Windows.Forms.TextBox txtFilename;
        private System.Windows.Forms.Button btnSendOne;
        private System.Windows.Forms.OpenFileDialog openFileDialog1;
        private System.Windows.Forms.Button btnBrowseOne;
        private System.Windows.Forms.TextBox txtDirectory;
        private System.Windows.Forms.Button btnSendDir;
        private System.Windows.Forms.ListBox lstWorkQueue;
        private System.Windows.Forms.TextBox edtSerialPort;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.CheckBox chkComOpen;
    }
}

