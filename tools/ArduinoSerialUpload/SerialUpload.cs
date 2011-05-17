using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.IO.Ports;

/*
 * ========== ========== Protocol Specification ========== ==========
 * Host = Arduino
 * Client = Uploader
 * 
 * Command Mode
 * Commmands are ASCII string data, terminiated with a newline character.  Commands 
 * can not exceed 127 bytes, including the newline character.
 * 
 * Startup:
 * 200 READY <page size> <page address bits>
 *   After host initialization (startup), it will send a READY message to 
 *   notify the client that the host is ready to receive commands.
 *   <page size> size of a dataflash page on the host
 *   <page address bits> number of bits in a dataflash page address
 * 
 * UPLOAD <start address> <size>\n
 * Client is requesting a file upload to dataflash.
 *   <start address> starting address in the host dataflash to write too
 *   <size> total size of the file to be received by the host
 * Responses:
 *   200 OK <address> <size> <page> <offset>\n
 *     Host will accept the transfer and responds with the parameter sent
 *     by the client, follwed by the flash page and offset the file will be 
 *     uploaded to.  If a 200 response code is sent, the host switches to 
 *     "upload mode" and will not accept commands until upload mode is exited
 *   501 <Error message>\n
 *     Host will not accept the file due to an invalid starting address
 *   502 <Error message>\n
 *     Host will not accept the file due to an invalid upload size
 *   500 <Error message>\n
 *     Host will not accept the file for another reason
 *   
 * Upload Mode
 * When in upload mode, the host will not respond to commands until upload
 * mode is exited.  Uploads are split into 126 byte blocks followed by a
 * 1 byte checksum.  If the block data matches the checksum, the host will
 * transmit the character 'Y' and the client can continue with the next block.
 * If the checksum does not match, tha host will respond with 'R' and the client
 * should retransmit the block and checksum.  If the host wishes to abort 
 * the transfer, it will send an 'A' and immediately return to command mode.
 * 
 * If the upload size (or remaining upload size) is smaller than a block,
 * the block should be truncated, with the checksum immediately following
 * the remaining data.  e.g. if there is 1 byte to transfer, the client
 * should send 1 byte followed by a 1 byte checksum and wait for the host
 * block response.
 * 
 * After the final block response from the host, the host will send
 * 200 OK <size>
 *   <size> size of the upload
 * 
 * Checksum
 * The checksum byte is a simple XOR combination of the data bytes
 * uint8_t checksum = 0
 * foreach (uint8_t b in data)
 *   checksum ^= b
 * 
 */
namespace ArduinoSerialUpload
{
    /*
    class EHCaller<T> where T : EventHandler
    {
        private event T _handler;
        public EHCaller(T handler)
        {
            _handler = handler;
        }
        public T Invoke(EventArgs e)
        {
            if (_handler != null)
                _handler(this, e);
            return _handler;
        }
    } 
     */

    class SerialUpload
    {
        private enum SendState
        {
            None,
            WaitingForReady,
            UploadRequested,
            BlockSent,
            WaitingForUploadOk
        };
        private class CompletedItem 
        {
            public string Name;
            public long Size;
            public int Address;
            public int Page;
            public int Offset;
        }

        private string _portName = "COM3";
        private int _portBaud = 115200;

        private int _hostPageSize;
        private int _hostAddressBits;

        private List<FileInfo> _workQueue = new List<FileInfo>();
        private List<CompletedItem> _completedList = new List<CompletedItem>();
        private SerialPort _port;
        private SendState _sendState;
        private int _hostAddress;
        private FileStream _currentFile;
        private byte[] _currentBlock = new byte[127];
        private int _currentBlockSize;

        public event EventHandler TransferComplete;

        public string PortName
        {
            get { return _portName; }
            set { _portName = value; }
        }

        public int PortBaud
        {
            get { return _portBaud; }
            set { _portBaud = value; }
        }

        public int HostAddressBits
        {
            get { return _hostAddressBits; }
        }
        public int HostPageSize
        {
            get { return _hostPageSize; }
        }

        SerialUpload()
        {

        }

        /* fileSpec can be a single filename, a directory name, or a wildcard */
        public void QueueFiles(string fileSpec)
        {
            string fileName = Path.GetFileName(fileSpec);
            string path = Path.GetDirectoryName(fileSpec);
            DirectoryInfo di = new DirectoryInfo(path);

            if (_workQueue.Count == 0)
                _workQueue = new List<FileInfo>(di.GetFiles(fileName));
            else
                foreach (FileInfo fi in di.GetFiles(fileName))
                    if (!_workQueue.Exists(p => p.FullName == fi.FullName))
                        _workQueue.Add(fi);
        }

        public int QueuedCount
        {
            get { return _workQueue.Count; }
        }

        public void BeginTransfer()
        {
            if (IsTransferring)
                return;

            if (_workQueue.Count == 0)
            {
                StopSend();
                return;
            }

            _sendState = SendState.WaitingForReady;
            _hostAddress = 0;

            _port = new SerialPort(_portName, _portBaud, Parity.None, 8, StopBits.One);
            _port.DataReceived += new SerialDataReceivedEventHandler(SerialDataReceived);
            _port.ReadTimeout = 250;
            _port.NewLine = "\n";
            _port.Open();
        }

        private void StopSend()
        {
            if (_port != null)
            {
                _port.Close();
                _port = null;
            }
            _sendState = SendState.None;
         
            // var e = new EHCaller<EventHandler>(TransferComplete).Invoke(EventArgs.Empty);
        }

        private string SerialReadLine()
        {
            try
            {
                return _port.ReadLine();
            }
            catch (TimeoutException)
            {
                return "";
            }
        }

        private void SerialDataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            switch (_sendState)
            {
                case SendState.None:
                    break;
                case SendState.WaitingForReady:
                    SerialResponseReady(SerialReadLine());
                    break;
                case SendState.UploadRequested:
                    SerialResponseUpload(SerialReadLine());
                    break;
                case SendState.BlockSent:
                    SerialResponseBlock(_port.ReadChar());
                    break;
                case SendState.WaitingForUploadOk:
                    SerialResponseUploadOk(SerialReadLine());
                    break;
                default:
                    break;
            }
        }

        private void SerialResponseUploadOk(string data)
        {
            if (String.IsNullOrWhiteSpace(data))
                return;
            if (data.StartsWith("200 OK "))
            {
                _workQueue.RemoveAt(0);
                RequestUpload();  
                return;
            }

            StopSend();
            throw new InvalidDataException("Upload not OK: " + data);
        }

        private void SerialResponseBlock(int c)
        {
            if (c == 'Y')
                ContinueFileUpload();
            else if (c == 'R')
                SendCurrentBlock();
            else if (c == 'A')
                StopSend();
        }

        private void SerialResponseUpload(string data)
        {
            if (String.IsNullOrWhiteSpace(data))
                return;

            string errMessage = "";
            if (data.StartsWith("200 OK "))
            {
                string[] vals = data.Split(new char[] { ' ' });
                if (vals.Length > 5)
                {
                    if (Convert.ToInt32(vals[2]) == _hostAddress
                        && Convert.ToInt32(vals[3]) == _workQueue[0].Length)
                    {
                        _completedList.Add(new CompletedItem()
                        {
                            Name = _workQueue[0].Name,
                            Size = _workQueue[0].Length,
                            Address = _hostAddress,
                            Page = Convert.ToInt32(vals[4]),
                            Offset = Convert.ToInt32(vals[5])
                        });

                        ContinueFileUpload();
                        return;
                    }
                }
                else
                    errMessage = "Invalid parameter count";
            } /* starts with ok */
            else if (data.StartsWith("500 ") || data.StartsWith("501 ") || data.StartsWith("502 "))
                errMessage = data.Substring(4);
            else
                errMessage = data;

            StopSend();
            throw new InvalidDataException("UPLOAD request failed " + errMessage);
        }

        private void ContinueFileUpload()
        {
            if (_currentFile == null)
                _currentFile = new FileStream(_workQueue[0].FullName, FileMode.Open, FileAccess.Read);

            _currentBlockSize = _currentFile.Read(_currentBlock, 0, _currentBlock.Length - 1);
            if (_currentBlockSize > 0)
            {
                _currentBlock[_currentBlockSize] = 0;
                for (int i = 0; i < _currentBlockSize; i++)
                    _currentBlock[_currentBlockSize] ^= _currentBlock[i];
                SendCurrentBlock();
            }
            else
            {
                _currentFile.Close();
                _currentFile = null;
                _sendState = SendState.WaitingForUploadOk;
            }
        }

        private void SendCurrentBlock()
        {
            _port.Write(_currentBlock, 0, _currentBlockSize + 1);
            _sendState = SendState.BlockSent;
        }

        private void SerialResponseReady(string data)
        {
            if (String.IsNullOrWhiteSpace(data))
                return;

            if (data.StartsWith("200 READY "))
            {
                string[] vals = data.Split(new char[] { ' ' });
                if (vals.Length > 3)
                {
                    _hostPageSize = Convert.ToInt32(vals[2]);
                    _hostAddressBits = Convert.ToInt32(vals[3]);
                    RequestUpload();
                    return;
                }

                StopSend();
                throw new InvalidDataException("READY response: " + data);
            }
        }

        private void RequestUpload()
        {
            if (_workQueue.Count == 0)
            {
                StopSend();
                return;
            }

            _port.WriteLine(String.Format("UPLOAD {0} {1}", _hostAddress, _workQueue[0].Length));
            _sendState = SendState.UploadRequested;
        }

        public bool IsTransferring
        {
            get { return _port != null; }
        }
    }
}
