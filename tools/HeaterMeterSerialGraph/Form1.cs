// HeaterMeter Copyright 2016 Bryan Mayland <bmayland@capnbry.net> 
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.Windows.Forms.DataVisualization.Charting;

namespace HeaterMeterSerialGraph
{
    public partial class Form1 : Form
    {
        private string _buff;
        private int _cnt = 0;

        public Form1()
        {
            InitializeComponent();
        }

        private void btnCOM_Click(object sender, EventArgs e)
        {
            serialPort.Close();
            serialPort.NewLine = "\n";
            serialPort.PortName = "COM" + txtComPort.Text;
            serialPort.Open();
        }

        private void handleLine(string s)
        {
            //70,67.6,67.6,0.0,-1.0,0.0,-1.0,0.0,-1.0,14,14,0
            string[] vals = s.Split(',');
            if (vals.Length < 12)
                return;
            AddVal(vals, 9, 0);
            AddVal(vals, 0, 1);
            AddVal(vals, 1, 2);
            AddVal(vals, 3, 3);
            AddVal(vals, 5, 4);
            ++_cnt;
        }

        private void AddVal(string[] vals, int valIdx, int seriesIdx)
        {
            double d;
            if (double.TryParse(vals[valIdx], out d) && ((seriesIdx == 0) || (d > 0.0f)))
                chart1.Invoke(new MethodInvoker(delegate { chart1.Series[seriesIdx].Points.AddXY(_cnt, d); }));
        }

        private void serialPort_DataReceived(object sender, System.IO.Ports.SerialDataReceivedEventArgs e)
        {
            _buff += serialPort.ReadExisting();   
            int pos = _buff.IndexOf('\n');
            while (pos != -1)
            {
                handleLine(_buff.Substring(0, pos));
                _buff = _buff.Substring(pos + 1).Trim();
                pos = _buff.IndexOf('\n');
            }
        }

        private void button1_Click(object sender, EventArgs e)
        {
            handleLine(@"70,67.6,67.6,0.0,-1.0,0.0,-1.0,0.0,-1.0,14,14,0");
        }

        private void textBox1_TextChanged(object sender, EventArgs e)
        {
            ((TextBox)sender).ForeColor = Color.Red;
        }

        private void textBox3_KeyPress(object sender, KeyPressEventArgs e)
        {
            if (e.KeyChar == '\r')
            {
                char c = ' ';
                if (sender == textBox1)
                    c = 'b';
                else if (sender == textBox2)
                    c = 'p';
                else if (sender == textBox4)
                    c = 'i';
                else if (sender == textBox3)
                    c = 'd';

                serialPort.WriteLine("/set?pid" + c + "=" + ((TextBox)sender).Text);
                ((TextBox)sender).ForeColor = Color.Black;
            }
        }

        private void Form1_FormClosed(object sender, FormClosedEventArgs e)
        {
            Properties.Settings.Default.Save();
        }

        private void clearChartToolStripMenuItem_Click(object sender, EventArgs e)
        {
            foreach (Series x in chart1.Series)
                x.Points.Clear();
        }
    }
}
