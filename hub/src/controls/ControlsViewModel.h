// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QSet>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

namespace ac {
// Display adapter only. Inputs never leave this object for a game/backend.
class ControlsViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
    Q_PROPERTY(QVariantList markers READ markers NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString gameId READ gameId NOTIFY changed)
    Q_PROPERTY(QString modelId READ modelId NOTIFY changed)
    Q_PROPERTY(QString notice READ notice NOTIFY changed)
    Q_PROPERTY(QUrl previewImage READ previewImage NOTIFY changed)
    Q_PROPERTY(bool previewAvailable READ previewAvailable NOTIFY changed)
    Q_PROPERTY(QString previewView READ previewView WRITE setPreviewView NOTIFY changed)
    Q_PROPERTY(QString primaryHand READ primaryHand WRITE setPrimaryHand NOTIFY changed)
    Q_PROPERTY(bool twoGunsActive READ twoGunsActive WRITE setTwoGunsActive NOTIFY changed)
    Q_PROPERTY(QString lastError READ lastError NOTIFY changed)
public:
    explicit ControlsViewModel(QObject *parent = nullptr);
    bool loadResolved(const QVariantMap &result, const QString &previewDirectory = {});
    bool loadResolvedJson(const QByteArray &json, const QString &previewDirectory = {});
    bool loadCatalogGame(const QByteArray &catalog, const QString &gameId, const QString &previewDirectory = {});
    Q_INVOKABLE void clear();
    Q_INVOKABLE void setBindingState(const QString &hand, const QString &control, bool pressed);
    Q_INVOKABLE void clearBindingStates();
    Q_INVOKABLE QVariantList controllerBindings(const QString &hand) const;
    Q_INVOKABLE QUrl controllerImage(const QString &hand) const;
    QVariantList rows() const { return m_rows; }
    QVariantList markers() const;
    QString title() const { return m_data.value("title").toString(); }
    QString gameId() const { return m_result.value("game_id").toString(); }
    QString modelId() const { return m_result.value("model").toString(); }
    QString notice() const;
    QUrl previewImage() const { return m_previewImage; }
    bool previewAvailable() const { return !m_previewImage.isEmpty(); }
    QString previewView() const { return m_view; }
    void setPreviewView(const QString &view);
    QString primaryHand() const { return m_primaryHand; }
    void setPrimaryHand(const QString &hand);
    bool twoGunsActive() const { return m_twoGunsActive; }
    void setTwoGunsActive(bool active);
    QString lastError() const { return m_error; }
signals:
    void changed();
private:
    void rebuild();
    void loadPreview();
    QVariantList makeRows() const;
    QString elementHand(const QVariantMap &element, const QVariantMap &binding) const;
    static QString bindingLabel(const QVariantMap &binding, const QString &hand);
    static QString stateKey(const QString &hand, const QString &control);
    QVariantMap m_result, m_data, m_preview, m_anchors;
    QVariantList m_rows;
    QString m_previewDirectory, m_error;
    QString m_view = "threequarter";
    QString m_primaryHand = "right";
    bool m_twoGunsActive = false;
    QUrl m_previewImage;
    QSet<QString> m_pressed;
};
}
