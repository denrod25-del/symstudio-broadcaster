#include "SymStudioAlertServer.hpp"

#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QByteArray>

static QString jsonEscape(const QString &in)
{
	QString out;
	out.reserve(in.size() + 8);
	for (const QChar c : in) {
		switch (c.unicode()) {
		case '\\': out += QStringLiteral("\\\\"); break;
		case '"': out += QStringLiteral("\\\""); break;
		case '\n': out += QStringLiteral("\\n"); break;
		case '\r': out += QStringLiteral("\\r"); break;
		case '\t': out += QStringLiteral("\\t"); break;
		default:
			if (c.unicode() < 0x20)
				out += QStringLiteral("\\u%1").arg(int(c.unicode()), 4, 16, QLatin1Char('0'));
			else
				out += c;
		}
	}
	return out;
}

SymStudioAlertServer::SymStudioAlertServer(QObject *parent) : QObject(parent)
{
	server = new QTcpServer(this);
	connect(server, &QTcpServer::newConnection, this, &SymStudioAlertServer::onNewConnection);
}

bool SymStudioAlertServer::start()
{
	if (server->isListening())
		return true;
	for (quint16 p = 28782; p <= 28792; ++p) {
		if (server->listen(QHostAddress::LocalHost, p)) {
			boundPort = p;
			return true;
		}
	}
	return false;
}

bool SymStudioAlertServer::isListening() const
{
	return server->isListening();
}

void SymStudioAlertServer::pushAlert(const QString &text, const QString &type)
{
	alertId++;
	alertText = text;
	alertType = type;
}

QString SymStudioAlertServer::alertJson() const
{
	return QStringLiteral("{\"id\":%1,\"text\":\"%2\",\"type\":\"%3\"}")
		.arg(alertId)
		.arg(jsonEscape(alertText), jsonEscape(alertType));
}

void SymStudioAlertServer::onNewConnection()
{
	while (QTcpSocket *sock = server->nextPendingConnection()) {
		connect(sock, &QTcpSocket::readyRead, sock, [this, sock]() {
			QByteArray req = sock->readAll();
			// Only need the request line: "GET <path> HTTP/1.1"
			const int sp1 = req.indexOf(' ');
			const int sp2 = sp1 >= 0 ? req.indexOf(' ', sp1 + 1) : -1;
			QString path = QStringLiteral("/");
			if (sp1 >= 0 && sp2 > sp1)
				path = QString::fromUtf8(req.mid(sp1 + 1, sp2 - sp1 - 1));

			QByteArray body;
			QByteArray ctype;
			if (path.startsWith(QStringLiteral("/alert"))) {
				body = alertJson().toUtf8();
				ctype = "application/json; charset=utf-8";
			} else {
				body = overlayHtml().toUtf8();
				ctype = "text/html; charset=utf-8";
			}

			QByteArray resp = "HTTP/1.1 200 OK\r\n";
			resp += "Content-Type: " + ctype + "\r\n";
			resp += "Cache-Control: no-store, no-cache, must-revalidate\r\n";
			resp += "Access-Control-Allow-Origin: *\r\n";
			resp += "Connection: close\r\n";
			resp += "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n";
			resp += body;

			sock->write(resp);
			sock->flush();
			sock->disconnectFromHost();
		});
		connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
	}
}

QString SymStudioAlertServer::overlayHtml() const
{
	return QStringLiteral(R"HTML(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<style>
  html,body{margin:0;height:100%;background:transparent;overflow:hidden;
    font-family:'Bahnschrift','Segoe UI',sans-serif;}
  #wrap{position:absolute;left:0;right:0;top:18%;display:flex;justify-content:center;}
  #card{min-width:520px;max-width:1100px;padding:26px 40px;border-radius:16px;
    background:linear-gradient(135deg,#0b1020 0%,#161c34 100%);
    border:2px solid #00E5FF;box-shadow:0 0 28px rgba(0,229,255,.55),0 0 60px rgba(255,0,200,.30);
    color:#EAF6FF;text-align:center;opacity:0;transform:translateX(-120%) scale(.6);}
  #card.show{animation:pop 5.5s cubic-bezier(.2,.9,.2,1) forwards;}
  #title{font-size:30px;font-weight:700;letter-spacing:.5px;
    background:linear-gradient(90deg,#00E5FF,#FF3DCB);-webkit-background-clip:text;
    background-clip:text;color:transparent;text-shadow:0 0 10px rgba(0,229,255,.25);}
  #msg{font-size:40px;font-weight:700;margin-top:8px;line-height:1.15;}
  @keyframes pop{
    0%{opacity:0;transform:translateX(-120%) scale(.6);}
    12%{opacity:1;transform:translateX(0) scale(1.12);}
    20%{transform:translateX(0) scale(1);}
    85%{opacity:1;transform:translateX(0) scale(1);}
    100%{opacity:0;transform:translateX(0) scale(.96);}
  }
</style>
</head>
<body>
<div id="wrap"><div id="card"><div id="title">NEW ALERT</div><div id="msg"></div></div></div>
<script>
  var lastId=null, card=document.getElementById('card'), msg=document.getElementById('msg');
  function chime(){
    try{
      var Ctx=window.AudioContext||window.webkitAudioContext; if(!Ctx) return;
      var ac=new Ctx(); var now=ac.currentTime;
      [[660,0],[990,0.13]].forEach(function(n){
        var o=ac.createOscillator(), g=ac.createGain();
        o.type='triangle'; o.frequency.value=n[0];
        o.connect(g); g.connect(ac.destination);
        var t=now+n[1];
        g.gain.setValueAtTime(0.0001,t);
        g.gain.exponentialRampToValueAtTime(0.32,t+0.03);
        g.gain.exponentialRampToValueAtTime(0.0001,t+0.45);
        o.start(t); o.stop(t+0.5);
      });
    }catch(e){}
  }
  function fire(text){
    msg.textContent=text;
    card.classList.remove('show'); void card.offsetWidth; card.classList.add('show');
    chime();
  }
  function poll(){
    fetch('/alert',{cache:'no-store'}).then(function(r){return r.json();}).then(function(a){
      if(lastId===null){lastId=a.id;return;}      // baseline on first load
      if(a.id!==lastId){lastId=a.id; fire(a.text||'Alert');}
    }).catch(function(){});
  }
  setInterval(poll,300); poll();
</script>
</body>
</html>)HTML");
}
