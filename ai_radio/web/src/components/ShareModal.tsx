import React from 'react';
import QRCode from 'qrcode.react';
import './ShareModal.css';

interface ShareModalProps {
  onClose: () => void;
  url: string;
}

const ShareModal: React.FC<ShareModalProps> = ({ onClose, url }) => {
  const copyToClipboard = () => {
    navigator.clipboard.writeText(url).then(() => {
      alert('URL copied to clipboard!');
    }, (err) => {
      console.error('Could not copy text: ', err);
    });
  };

  return (
    <div className="modal-overlay" onClick={onClose}>
      <div className="modal-content" onClick={(e) => e.stopPropagation()}>
        <div className="modal-header">
          <h2>Share Station</h2>
          <button onClick={onClose} className="close-button">&times;</button>
        </div>
        <div className="modal-body">
          <p>Scan this QR code with your phone to control the station remotely.</p>
          <div className="qr-code-container">
            <QRCode value={url} size={256} />
          </div>
          <p>Or use this URL on your local network:</p>
          <div className="url-container">
            <input type="text" value={url} readOnly />
            <button onClick={copyToClipboard}>Copy</button>
          </div>
        </div>
      </div>
    </div>
  );
};

export default ShareModal;
