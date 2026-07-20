#include "SymStudioMultistreamDock.hpp"
#include "OBSApp.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QMetaObject>

#include <obs.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>

struct Preset {
	const char *name;
	const char *url;
};
static const Preset kPresets[] = {
	{"Custom", ""},
	{"Twitch", "rtmp://live.twitch.tv/app"},
	{"YouTube", "rtmp://a.rtmp.youtube.com/live2"},
	{"Facebook", "rtmps://live-api-s.facebook.com:443/rtmp/"},
	{"Trovo", "rtmp://livepush.trovo.live/live/"},
	{"Kick", "rtmps://"}, // Kick gives you a region-specific ingest URL; paste it
};

static void MultiFrontendEvent(enum obs_frontend_event ev, void *data)
{
	auto *self = (SymStudioMultistreamDock *)data;
	if (ev == OBS_FRONTEND_EVENT_STREAMING_STARTED)
		QMetaObject::invokeMethod(self, "onStreamStarted", Qt::QueuedConnection);
	else if (ev == OBS_FRONTEND_EVENT_STREAMING_STOPPING || ev == OBS_FRONTEND_EVENT_STREAMING_STOPPED)
		QMetaObject::invokeMethod(self, "onStreamStopping", Qt::QueuedConnection);
}

SymStudioMultistreamDock::SymStudioMultistreamDock(QWidget *parent) : QWidget(parent)
{
	QVBoxLayout *root = new QVBoxLayout(this);
	root->setContentsMargins(8, 8, 8, 8);
	root->setSpacing(6);

	QLabel *hdr = new QLabel(QStringLiteral("Extra destinations go live when you Start Streaming "
						"(they share your main stream's quality)."),
				 this);
	hdr->setWordWrap(true);
	hdr->setStyleSheet(QStringLiteral("color:#9AA3B2;font-size:12px;"));
	root->addWidget(hdr);

	QWidget *rowsHost = new QWidget(this);
	rowsLayout = new QVBoxLayout(rowsHost);
	rowsLayout->setContentsMargins(0, 0, 0, 0);
	rowsLayout->setSpacing(6);
	rowsLayout->addStretch(1);
	QScrollArea *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setWidget(rowsHost);
	root->addWidget(scroll, 1);

	QPushButton *addBtn = new QPushButton(QStringLiteral("Add destination"), this);
	root->addWidget(addBtn);
	connect(addBtn, &QPushButton::clicked, this, &SymStudioMultistreamDock::addRow);

	QLabel *warn = new QLabel(QStringLiteral("Heads up: each destination is a full extra upload — "
						 "3 platforms ≈ 3× your upload speed."),
				  this);
	warn->setWordWrap(true);
	warn->setStyleSheet(QStringLiteral("color:#5F6B7C;font-size:11px;"));
	root->addWidget(warn);

	setLayout(root);

	pollTimer = new QTimer(this);
	pollTimer->setInterval(2000);
	connect(pollTimer, &QTimer::timeout, this, &SymStudioMultistreamDock::pollStatus);

	loadDests();
	obs_frontend_add_event_callback(MultiFrontendEvent, this);
}

void SymStudioMultistreamDock::buildRow(const QString &name, const QString &url, const QString &key, bool enabled)
{
	Row *r = new Row();
	r->w = new QWidget(this);
	QVBoxLayout *v = new QVBoxLayout(r->w);
	v->setContentsMargins(8, 6, 8, 6);
	v->setSpacing(4);
	r->w->setStyleSheet(
		QStringLiteral("QWidget{background:#10141E;border:1px solid #1A2230;border-radius:8px;}"));

	QHBoxLayout *top = new QHBoxLayout();
	r->en = new QCheckBox(r->w);
	r->en->setChecked(enabled);
	r->name = new QLineEdit(name, r->w);
	r->name->setPlaceholderText(QStringLiteral("name (e.g. YouTube)"));
	r->status = new QLabel(QStringLiteral("idle"), r->w);
	r->status->setStyleSheet(QStringLiteral("color:#7E8796;font-size:11px;"));
	QPushButton *rm = new QPushButton(QStringLiteral("✕"), r->w);
	rm->setFixedWidth(28);
	top->addWidget(r->en);
	top->addWidget(r->name, 1);
	top->addWidget(r->status);
	top->addWidget(rm);
	v->addLayout(top);

	QHBoxLayout *mid = new QHBoxLayout();
	r->preset = new QComboBox(r->w);
	for (const auto &p : kPresets)
		r->preset->addItem(QString::fromUtf8(p.name));
	r->url = new QLineEdit(url, r->w);
	r->url->setPlaceholderText(QStringLiteral("rtmp:// server URL"));
	mid->addWidget(r->preset);
	mid->addWidget(r->url, 1);
	v->addLayout(mid);

	r->key = new QLineEdit(key, r->w);
	r->key->setPlaceholderText(QStringLiteral("stream key"));
	r->key->setEchoMode(QLineEdit::Password);
	v->addWidget(r->key);

	rowsLayout->insertWidget(rowsLayout->count() - 1, r->w);
	rows.append(r);

	auto save = [this]() { saveDests(); };
	connect(r->en, &QCheckBox::toggled, this, save);
	connect(r->name, &QLineEdit::editingFinished, this, save);
	connect(r->url, &QLineEdit::editingFinished, this, save);
	connect(r->key, &QLineEdit::editingFinished, this, save);
	connect(r->preset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, r](int i) {
		const QString u = QString::fromUtf8(kPresets[i].url);
		if (!u.isEmpty())
			r->url->setText(u);
		saveDests();
	});
	connect(rm, &QPushButton::clicked, this, [this, r]() {
		rows.removeOne(r);
		r->w->deleteLater();
		delete r;
		saveDests();
	});
}

void SymStudioMultistreamDock::addRow()
{
	buildRow(QString(), QString(), QString(), true);
}

void SymStudioMultistreamDock::saveDests()
{
	obs_data_array_t *arr = obs_data_array_create();
	for (Row *r : rows) {
		obs_data_t *d = obs_data_create();
		obs_data_set_string(d, "name", r->name->text().toUtf8().constData());
		obs_data_set_string(d, "url", r->url->text().trimmed().toUtf8().constData());
		obs_data_set_string(d, "key", r->key->text().trimmed().toUtf8().constData());
		obs_data_set_bool(d, "enabled", r->en->isChecked());
		obs_data_array_push_back(arr, d);
		obs_data_release(d);
	}
	obs_data_t *wrap = obs_data_create();
	obs_data_set_array(wrap, "dests", arr);
	config_set_string(App()->GetUserConfig(), "SymStudioMultistream", "Destinations", obs_data_get_json(wrap));
	config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
	obs_data_release(wrap);
	obs_data_array_release(arr);
}

void SymStudioMultistreamDock::loadDests()
{
	const char *j = config_get_string(App()->GetUserConfig(), "SymStudioMultistream", "Destinations");
	if (!j || !*j)
		return;
	obs_data_t *wrap = obs_data_create_from_json(j);
	obs_data_array_t *arr = obs_data_get_array(wrap, "dests");
	const size_t n = arr ? obs_data_array_count(arr) : 0;
	for (size_t i = 0; i < n; i++) {
		obs_data_t *d = obs_data_array_item(arr, i);
		buildRow(QString::fromUtf8(obs_data_get_string(d, "name")),
			 QString::fromUtf8(obs_data_get_string(d, "url")),
			 QString::fromUtf8(obs_data_get_string(d, "key")), obs_data_get_bool(d, "enabled"));
		obs_data_release(d);
	}
	obs_data_array_release(arr);
	obs_data_release(wrap);
}

SymStudioMultistreamDock::~SymStudioMultistreamDock()
{
	obs_frontend_remove_event_callback(MultiFrontendEvent, this);
	stopExtras();
}

void SymStudioMultistreamDock::startExtras()
{
	obs_output_t *mainOut = obs_frontend_get_streaming_output();
	if (!mainOut)
		return;
	obs_encoder_t *venc = obs_output_get_video_encoder(mainOut);
	obs_encoder_t *aenc = obs_output_get_audio_encoder(mainOut, 0);
	if (!venc || !aenc) {
		obs_output_release(mainOut);
		return;
	}

	for (Row *r : rows) {
		if (!r->en->isChecked())
			continue;
		const QString url = r->url->text().trimmed();
		const QString key = r->key->text().trimmed();
		const QString nm = r->name->text().trimmed().isEmpty() ? QStringLiteral("dest")
								      : r->name->text().trimmed();
		if (url.isEmpty() || key.isEmpty()) {
			r->status->setText(QStringLiteral("skipped (no URL/key)"));
			continue;
		}
		obs_data_t *ss = obs_data_create();
		obs_data_set_string(ss, "server", url.toUtf8().constData());
		obs_data_set_string(ss, "key", key.toUtf8().constData());
		obs_service_t *svc =
			obs_service_create("rtmp_custom", (nm + " service").toUtf8().constData(), ss, nullptr);
		obs_data_release(ss);

		obs_output_t *out = obs_output_create("rtmp_output", nm.toUtf8().constData(), nullptr, nullptr);
		obs_output_set_service(out, svc);
		obs_output_set_video_encoder(out, venc);
		obs_output_set_audio_encoder(out, aenc, 0);

		const bool ok = obs_output_start(out);
		r->status->setText(ok ? QStringLiteral("connecting…") : QStringLiteral("failed to start"));
		extras.append({out, svc, nm, 0});
	}

	obs_output_release(mainOut);
	if (!extras.isEmpty())
		pollTimer->start();
}

void SymStudioMultistreamDock::stopExtras()
{
	pollTimer->stop();
	for (Extra &e : extras) {
		if (e.out) {
			obs_output_stop(e.out);
			obs_output_release(e.out);
		}
		if (e.svc)
			obs_service_release(e.svc);
	}
	extras.clear();
	for (Row *r : rows)
		r->status->setText(QStringLiteral("idle"));
}

void SymStudioMultistreamDock::onStreamStarted()
{
	streaming = true;
	startExtras();
}

void SymStudioMultistreamDock::onStreamStopping()
{
	streaming = false;
	stopExtras();
}

void SymStudioMultistreamDock::pollStatus()
{
	for (Extra &e : extras) {
		Row *row = nullptr;
		for (Row *r : rows) {
			const QString nm = r->name->text().trimmed().isEmpty() ? QStringLiteral("dest")
									       : r->name->text().trimmed();
			if (nm == e.name) {
				row = r;
				break;
			}
		}
		if (!row)
			continue;
		if (e.out && obs_output_active(e.out)) {
			row->status->setText(QStringLiteral("● live"));
			row->status->setStyleSheet(QStringLiteral("color:#39FF6A;font-size:11px;"));
		} else {
			e.polls++;
			if (e.polls > 5) {
				row->status->setText(QStringLiteral("error (check URL/key)"));
				row->status->setStyleSheet(QStringLiteral("color:#FF5046;font-size:11px;"));
			}
		}
	}
}
