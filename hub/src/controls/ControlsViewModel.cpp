// SPDX-License-Identifier: GPL-3.0-only
#include "ControlsViewModel.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QImageReader>
#include <QRegularExpression>
#include <cmath>

namespace ac {
namespace {
QString pretty(QString value) {
    value.replace('-', ' ');
    value.replace('_', ' ');
    if (!value.isEmpty()) value[0] = value[0].toUpper();
    return value;
}
QString confinedFile(const QString &directory, const QString &basename) {
    if (directory.isEmpty() || basename.isEmpty() || QFileInfo(basename).fileName() != basename ||
        basename.contains('\\') || basename.contains('/') || basename == "." || basename == "..") return {};
    const QString root = QFileInfo(directory).canonicalFilePath();
    const QFileInfo file(QDir(directory).filePath(basename));
    const QString path = file.canonicalFilePath();
    // QFileInfo canonical paths use forward slashes on every Qt platform.
#ifdef Q_OS_WIN
    constexpr auto sensitivity = Qt::CaseInsensitive;
#else
    constexpr auto sensitivity = Qt::CaseSensitive;
#endif
    if (root.isEmpty() || path.isEmpty() || !file.isFile() ||
        !path.startsWith(root + '/', sensitivity)) return {};
    return path;
}
bool coordinates(const QVariant &value) {
    const auto p = value.toList();
    if (p.size() != 2) return false;
    for (const auto &v : p) {
        bool valid = false;
        const double n = v.toDouble(&valid);
        if (!valid || !std::isfinite(n) || n < 0 || n > 1) return false;
    }
    return true;
}
}

ControlsViewModel::ControlsViewModel(QObject *parent) : QObject(parent) {}
QString ControlsViewModel::stateKey(const QString &hand, const QString &control) { return hand + '/' + control; }

bool ControlsViewModel::loadResolvedJson(const QByteArray &json, const QString &directory) {
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        clear(); m_error = "Controls data could not be read"; emit changed(); return false;
    }
    auto result = document.object().toVariantMap();
    if (result.contains("games")) {
        const auto games = result.value("games").toList();
        if (games.size() != 1 || !result.value("errors").toList().isEmpty()) {
            clear(); m_error = "Choose one game with valid controls data"; emit changed(); return false;
        }
        result = games.front().toMap();
    }
    return loadResolved(result, directory);
}

bool ControlsViewModel::loadResolved(const QVariantMap &result, const QString &directory) {
    clear();
    const auto data = result.value("data").toMap();
    static const QRegularExpression id("^[a-z0-9]+(?:-[a-z0-9]+)*$");
    if (data.value("version").toString() != "0.1" || !id.match(result.value("model").toString()).hasMatch() ||
        result.value("game_id").toString().isEmpty() || data.value("title").toString().isEmpty()) {
        m_error = "Controls data is unavailable"; emit changed(); return false;
    }
    QSet<QString> ids;
    for (const QString &key : {QString("element"), QString("unmapped_part")}) {
        for (const auto &value : data.value(key).toList()) {
            const auto row = value.toMap();
            const QString rowId = row.value("id").toString();
            if (rowId.isEmpty() || ids.contains(rowId) || row.value("label").toString().isEmpty()) {
                m_error = "Controls contain an invalid or duplicate part"; emit changed(); return false;
            }
            ids.insert(rowId);
        }
    }
    m_result = result; m_data = data; m_previewDirectory = directory;
    const QString profileHand = data.value("policy").toMap().value("p1_hand").toString();
    if (profileHand == "left" || profileHand == "right") m_primaryHand = profileHand;
    // Backend capacity does not mean player two has joined. The host explicitly
    // sets twoGunsActive when choosing a dual-hand preview or after joining.
    m_twoGunsActive = false;
    const QString defaultHold=data.value("assembly").toMap().value("default_hold").toString();
    m_holdMode = defaultHold=="docked" ? "docked" : "split";
    loadPreview(); rebuild(); return true;
}

bool ControlsViewModel::loadCatalogGame(const QByteArray &catalog, const QString &gid, const QString &directory) {
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(catalog,&error);
    if (error.error == QJsonParseError::NoError && document.isObject() &&
        document.object().value("version").toString() == "0.1") {
        const auto games = document.object().value("games").toArray();
        for (const auto &game : games) {
            if (game.toObject().value("game_id").toString() == gid)
                return loadResolved(game.toObject().toVariantMap(),directory);
        }
    }
    clear(); m_error = "No controls data for this game"; emit changed(); return false;
}

void ControlsViewModel::clear() {
    m_result.clear(); m_data.clear(); m_rows.clear(); m_preview.clear(); m_anchors.clear();
    m_previewImage = QUrl{}; m_previewDirectory.clear(); m_pressed.clear(); m_error.clear(); m_twoGunsActive = false;
    m_primaryHand = "right";
    m_holdMode = "split"; m_accessoryAnchors.clear(); m_accessoryImage = QUrl{};
    emit changed();
}

void ControlsViewModel::loadPreview() {
    m_previewImage = QUrl{}; m_anchors.clear(); m_preview.clear();
    m_accessoryImage=QUrl{}; m_accessoryAnchors.clear();
    const QString manifest = confinedFile(m_previewDirectory, modelId() + ".json");
    if (manifest.isEmpty()) return;
    QFile file(manifest);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) return;
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return;
    const auto preview = document.object().toVariantMap();
    const auto size = preview.value("size").toList();
    if (preview.value("version").toString() != "0.1" || preview.value("model").toString() != modelId() ||
        size != QVariantList{512,512}) return;
    const auto view = preview.value("views").toMap().value(m_view).toMap();
    const QString image = confinedFile(m_previewDirectory, view.value("image").toString());
    QImageReader reader(image);
    if (image.isEmpty() || reader.size() != QSize(512,512) || !reader.canRead()) return;
    const auto anchors = view.value("nodes").toMap();
    for (auto it = anchors.begin(); it != anchors.end(); ++it) if (!coordinates(it.value())) return;
    m_preview = preview; m_anchors = anchors; m_previewImage = QUrl::fromLocalFile(image);
    if (!accessoryId().isEmpty()) {
        const QString accessoryManifest=confinedFile(m_previewDirectory,accessoryId()+".json");
        QFile accessoryFile(accessoryManifest);
        if (!accessoryFile.open(QIODevice::ReadOnly)||accessoryFile.size()>1024*1024) return;
        const auto accessoryDocument=QJsonDocument::fromJson(accessoryFile.readAll(),&error);
        if(error.error!=QJsonParseError::NoError||!accessoryDocument.isObject()) return;
        const auto accessory=accessoryDocument.object().toVariantMap();
        if(accessory.value("model").toString()!=accessoryId()||accessory.value("version").toString()!="0.1"||accessory.value("size").toList()!=QVariantList{512,512}) return;
        const auto accessoryView=accessory.value("views").toMap().value(m_view).toMap();
        const QString accessoryPath=confinedFile(m_previewDirectory,accessoryView.value("image").toString());
        QImageReader accessoryReader(accessoryPath);
        if(accessoryPath.isEmpty()||accessoryReader.size()!=QSize(512,512)||!accessoryReader.canRead()) return;
        const auto accessoryAnchors=accessoryView.value("nodes").toMap();
        for(auto it=accessoryAnchors.begin();it!=accessoryAnchors.end();++it) if(!coordinates(it.value())) return;
        m_accessoryAnchors=accessoryAnchors; m_accessoryImage=QUrl::fromLocalFile(accessoryPath);
    }
}

QString ControlsViewModel::elementHand(const QVariantMap &element, const QVariantMap &binding) const {
    if (wiiControls()) {
        if(m_data.value("schema").toString()=="shell-stick/1") {
            const QString firing=element.value("slot").toInt()==0 ? m_primaryHand : (m_primaryHand=="right"?"left":"right");
            return binding.value("role").toString()=="nunchuk"&&!m_twoGunsActive ? (firing=="right"?"left":"right") : firing;
        }
        const auto device=binding.value("physical_dsu_slot_by_mode").toMap().value(m_twoGunsActive ? "two_remotes" : "one_gun");
        if (!device.isNull() && device.isValid()) return device.toInt()==0 ? "right" : "left";
    }
    const QString hand = binding.value("hand").toString();
    if (hand != "slot") return hand;
    return element.value("slot").toInt() == 0 ? m_primaryHand : (m_primaryHand == "right" ? "left" : "right");
}
QString ControlsViewModel::bindingLabel(const QVariantMap &binding, const QString &hand) {
    const QString control = binding.value("control").toString();
    if(control.contains(" & ")) {
        QStringList labels;
        for(const auto &raw:control.split(" & ")) {
            const auto token=raw.trimmed(); if(token.startsWith('!')) continue;
            labels.append(bindingLabel({{"control",token}},hand));
        }
        return labels.join(" + ");
    }
    const QString side = hand == "either" ? "Either hand" : pretty(hand);
    QString label;
    if (control == "primary") label = hand == "right" ? "A (right)" : hand == "left" ? "X (left)" : "A / X";
    else if (control == "secondary") label = hand == "right" ? "B (right)" : hand == "left" ? "Y (left)" : "B / Y";
    else if (control == "offscreen_trigger") label = side + " trigger, aimed off-screen";
    else if (control == "flick_up") label = side + " wrist flick up";
    else if (control == "pump") label = "Pump with free hand";
    else if (control == "slide") label = "Slide with free hand";
    else if (control == "thumbstick_click") label = side + " thumbstick click";
    else if (control == "menu_chord") label = "Menu chord";
    else if (control == "pointer") label = side + " gun direction";
    else if (control == "thumbstick") label = side + " thumbstick";
    else if (control == "click") label = side + " thumbstick click";
    else if (control == "up" || control == "down" || control == "left" || control == "right") label = side + " thumbstick " + control;
    else if (control == "menu") label = side + " menu button";
    else if (control == "remote_motion") label = side + " gun movement";
    else if (control == "nunchuk_motion") label = side + " controller movement";
    else if (control == "reload_fallback") label = "Keyboard R / Ctrl+Alt+R (rebindable)";
    else if (control == "melee_fallback") label = "Keyboard V / Ctrl+Alt+V (rebindable)";
    else if (control == "dpad") label = "Keyboard arrows / directional binding";
    else if (control == "keyboard_minus") label = "Keyboard - / custom binding";
    else if (control == "keyboard_one") label = "Keyboard 1 / custom binding";
    else if (control == "keyboard_two") label = "Keyboard 2 / custom binding";
    else if (control == "owner_setup") label = "Choose in the headset sitting";
    else if (control == "trigger" || control == "grip") label = side + ' ' + control;
    else return "Binding pending";
    if (binding.value("mode").toString() == "hold") label += " (hold)";
    if (binding.value("mode").toString() == "toggle") label += " (toggle)";
    return label;
}

QVariantList ControlsViewModel::makeRows() const {
    QVariantList result;
    auto add = [this,&result](const QVariantMap &source, bool unmapped) {
        if (wiiControls() && source.value("slot").toInt()>0 && !m_twoGunsActive) return;
        const auto ordinary = source.value("binding").toMap();
        const auto fallback = source.value("fallback_binding").toMap();
        auto binding = m_twoGunsActive && !fallback.isEmpty() ? fallback : ordinary;
        const auto expressions=binding.value("expression_by_mode").toMap();
        if(!expressions.isEmpty()) binding["control"]=expressions.value(m_twoGunsActive?"two_remotes":m_holdMode);
        const QString hand = elementHand(source,binding);
        const QString control = binding.value("control").toString();
        const auto availability = source.value("availability").toMap();
        QString state = availability.value("state").toString();
        QString reason = availability.value("reason").toString();
        if (wiiControls()) reason=source.value("player_reason","This control is not available yet.").toString();
        const bool unsupportedHand = wiiControls() && !m_data.value("policy").toMap().value("supported_primary_hands").toStringList().contains(m_primaryHand);
        if (unmapped) state = "unmapped";
        else if (unsupportedHand) {
            state = "unavailable";
            reason = "Left-hand Wii Remote setup is not available yet.";
        }
        else if (state != "available" && state != "unavailable" && state != "planned") state = "unverified";
        const QString node = source.value("node").toString();
        const QString partModel=source.value("model",modelId()).toString();
        const auto anchor = (partModel==accessoryId()?m_accessoryAnchors:m_anchors).value(node).toList();
        bool expressionPressed=true;
        for(const auto &raw:control.split(" & ")) {
            const auto token=raw.trimmed(); const bool neg=token.startsWith('!');
            const bool held=m_pressed.contains(stateKey(hand,neg?token.mid(1):token));
            expressionPressed &= neg?!held:held;
        }
        const bool recenter=m_pressed.contains(stateKey("left","click"))&&m_pressed.contains(stateKey("right","click"));
        const bool pressed = !recenter && !unmapped && state != "planned" && state != "unavailable" && (expressionPressed || m_pressed.contains(stateKey(hand,control)) ||
            (hand == "either" && (m_pressed.contains(stateKey("left",control)) || m_pressed.contains(stateKey("right",control)))));
        const QString part = unmapped ? source.value("label").toString() : pretty(source.value("part").toString());
        const QString action = unmapped ? "Game action pending" : source.value("label").toString();
        const QString bindingText = unmapped ? "Binding pending" : source.contains("binding_label") ? source.value("binding_label").toString() : bindingLabel(binding,hand);
        const QString availabilityLabel = state == "planned" ? "Planned — unavailable" : state == "unavailable" ? "Unavailable" :
                                          wiiControls() ? "Wii control mapping — awaiting gameplay validation" : state == "available" ? "Ready to test" :
                                          state == "unmapped" ? "Mapping pending" : "Awaiting validation";
        const auto device = binding.value("physical_dsu_slot_by_mode").toMap().value(m_twoGunsActive ? "two_remotes" : "one_gun");
        QVariantMap row{{"id",source.value("id")},{"part",part},{"action",action},{"node",node},{"model",partModel},
            {"slot",source.value("slot")},{"player",source.value("player")},{"hand",hand},{"control",control},
            {"physicalDsuSlot",m_data.value("schema").toString()=="shell-stick/1" ? (hand=="right"?0:1) : device.isNull() || !device.isValid() ? -1 : device.toInt()},
            {"bindingLabel",bindingText},{"fallbackLabel",fallback.isEmpty() ? QString() : bindingLabel(fallback,elementHand(source,fallback))},
            {"availability",state},{"availabilityLabel",availabilityLabel},{"reason",unmapped ? source.value("reason") : reason},
            {"pressed",pressed},{"warning",source.value("warning",false)},{"anchorAvailable",previewAvailable() && coordinates(anchor)},
            {"anchorX",anchor.size()==2 ? anchor[0] : QVariant(0.0)},{"anchorY",anchor.size()==2 ? anchor[1] : QVariant(0.0)},
            {"callout",part + " → " + action + " → " + bindingText}};
        result.append(row);
    };
    for (const auto &row : m_data.value("element").toList()) add(row.toMap(),false);
    for (const auto &row : m_data.value("unmapped_part").toList()) add(row.toMap(),true);
    return result;
}
void ControlsViewModel::rebuild() { m_rows = makeRows(); emit changed(); }
QString ControlsViewModel::proposalNotice() const {
    const auto proposal=m_data.value("gun_proposal").toMap();
    const auto reload=m_data.value("reload").toMap();
    if(proposal.isEmpty()) return {};
    return "World gun proposal: "+proposal.value("model").toString()+" (owner choice pending). Controls illustration: shell and stick. Default hold: "+m_data.value("assembly").toMap().value("default_hold").toString()+
        ". Reload choices: "+reload.value("choices").toStringList().join(", ")+". Starting choice: "+reload.value("selected").toString()+" — question; game layout and motion recognition pending. Button fallback remains in the shared mapping.";
}
QString ControlsViewModel::notice() const {
    if (!m_error.isEmpty()) return m_error;
    if (wiiControls()) {
        if(m_data.value("schema").toString()=="shell-stick/1") return
            "Route: "+m_data.value("route").toString()+" · Hold: "+holdMode()+" · Firing hand: "+m_primaryHand+
            (m_twoGunsActive ? ". A shell at each controller; no separate stick." : m_holdMode=="docked" ? ". Stick seated at the shell's rear grip; two-handed aim." : ". Shell in firing hand; stick in the other.")+
            " Both thumbstick clicks recenter. Game layout/actions remain questions. Native two-player aim is unsupported; native left aim requires its matching installed hand setting.";
        if (!m_data.value("policy").toMap().value("supported_primary_hands").toStringList().contains(m_primaryHand))
            return "Left-hand Wii Remote setup is not available yet. Right hand: Wii Remote. Left hand: Nunchuk.";
        return (m_twoGunsActive ? QString("Player 1: right hand. Player 2: left hand. Each hand controls its own Wii Remote and Nunchuk.") :
                                 QString("Right hand: Wii Remote. Left hand: Nunchuk.")) +
            " Some controls are unavailable.";
    }
    if (m_rows.isEmpty()) return "Choose a game to see its controls";
    for (const auto &value : m_rows) if (value.toMap().value("availability").toString() != "available")
        return "Some controls are awaiting validation. Their mappings are shown for review.";
    return "Controls match the supplied input declarations. Gameplay validation is separate.";
}
void ControlsViewModel::setPreviewView(const QString &view) {
    if ((view != "front" && view != "threequarter" && view != "held") || m_view == view) return;
    m_view = view; loadPreview(); rebuild();
}
void ControlsViewModel::setPrimaryHand(const QString &hand) {
    if ((hand != "left" && hand != "right") || hand == m_primaryHand) return;
    m_primaryHand = hand; m_pressed.clear(); rebuild();
}
void ControlsViewModel::setTwoGunsActive(bool active) {
    if(active&&m_data.value("route").toString()=="dolphinxr"&&!m_data.value("policy").toMap().value("native_two_remotes").toBool()) return;
    if (active == m_twoGunsActive || (active && m_result.value("configured_slots").toInt() < 2)) return;
    m_twoGunsActive = active; m_pressed.clear(); rebuild();
}
void ControlsViewModel::setHoldMode(const QString &mode) {
    if(mode=="two-remotes") { setTwoGunsActive(true); return; }
    if(mode!="split"&&mode!="docked") return;
    setTwoGunsActive(false); m_holdMode=mode; m_pressed.clear(); rebuild();
}
void ControlsViewModel::setBindingState(const QString &hand, const QString &control, bool pressed) {
    if (hand != "left" && hand != "right") return;
    const QString key = stateKey(hand,control);
    if (pressed == m_pressed.contains(key)) return;
    if (pressed) m_pressed.insert(key); else m_pressed.remove(key);
    rebuild();
}
void ControlsViewModel::clearBindingStates() { if (!m_pressed.isEmpty()) { m_pressed.clear(); rebuild(); } }

QVariantList ControlsViewModel::controllerBindings(const QString &hand) const {
    QVariantList result;
    for (const auto &value : m_rows) {
        auto row = value.toMap();
        if (wiiControls() && (row.value("availability").toString()=="planned" || row.value("availability").toString()=="unavailable")) continue;
        if (row.value("availability").toString() == "unmapped" ||
            (row.value("hand").toString() != hand && row.value("hand").toString() != "either")) continue;
        const QString control = row.value("control").toString();
        if(m_data.value("schema").toString()=="shell-stick/1") {
            for(const auto &raw:control.split(" & ")) {
                const auto token=raw.trimmed(); if(token.startsWith('!')) continue;
                auto part=row; QString highlight=token;
                if(token=="click"||token=="up"||token=="down"||token=="left"||token=="right") highlight="thumbstick_click";
                part["controllerControl"]=highlight;result.append(part);
            }
            continue;
        }
        QString highlight = control;
        if (highlight == "offscreen_trigger") highlight = "trigger";
        if (wiiControls() && highlight == "thumbstick") highlight = "thumbstick_click";
        row["controllerControl"] = highlight;
        result.append(row);
    }
    return result;
}
QVariantList ControlsViewModel::markers() const {
    return markersForModel(modelId(),m_data.value("schema").toString()=="shell-stick/1"?0:-1);
}
QVariantList ControlsViewModel::accessoryMarkers() const { return markersForModel(accessoryId()); }
QVariantList ControlsViewModel::markersForModel(const QString &model,int player) const {
    QVariantList result;
    QMap<QString,QVariantMap> nodes;
    int number = 0;
    for (const auto &value : m_rows) {
        ++number;
        const auto row = value.toMap();
        if(player>=0&&row.value("player").toInt()!=player) continue;
        if(row.value("model").toString()!=model) continue;
        if (!row.value("anchorAvailable").toBool()) continue;
        const QString node = row.value("node").toString();
        auto marker = nodes.value(node);
        if (marker.isEmpty()) marker = {{"node",node},{"x",row.value("anchorX")},{"y",row.value("anchorY")},
                                       {"numbers",QString()},{"callout",QString()},{"pressed",false}};
        marker["numbers"] = marker.value("numbers").toString() + (marker.value("numbers").toString().isEmpty() ? "" : ", ") + QString::number(number);
        marker["callout"] = marker.value("callout").toString() + (marker.value("callout").toString().isEmpty() ? "" : "\n") + row.value("callout").toString();
        marker["pressed"] = marker.value("pressed").toBool() || row.value("pressed").toBool();
        nodes[node] = marker;
    }
    for (const auto &marker : nodes) result.append(marker);
    return result;
}
QUrl ControlsViewModel::controllerImage(const QString &hand) const {
    return QUrl(hand == "left" ? "qrc:/controls/icons/touch-left.svg" : "qrc:/controls/icons/touch-right.svg");
}
}
