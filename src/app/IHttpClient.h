#pragma once

#include <QByteArray>
#include <QString>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

class QNetworkAccessManager;

// Minimal HTTP abstraction so GoogleAuth/GoogleDriveSync can be unit tested
// without touching the network. Production code uses QtHttpClient; tests
// substitute a fake that returns canned responses for known URLs.
class IHttpClient
{
public:
    struct Response
    {
        int statusCode = 0;
        QByteArray body;
        QString errorString; // non-empty only on a transport-level failure (no HTTP response at all)
    };

    using Headers = std::vector<std::pair<QString, QString>>;
    using Callback = std::function<void(const Response &)>;

    virtual ~IHttpClient() = default;

    virtual void get(const QString &url, const Headers &headers, Callback callback) = 0;
    virtual void post(const QString &url, const Headers &headers, const QByteArray &body,
                       const QString &contentType, Callback callback) = 0;
    virtual void patch(const QString &url, const Headers &headers, const QByteArray &body,
                        const QString &contentType, Callback callback) = 0;
};

// QNetworkAccessManager-backed implementation used outside of tests.
class QtHttpClient : public IHttpClient
{
public:
    QtHttpClient();
    ~QtHttpClient() override;

    void get(const QString &url, const Headers &headers, Callback callback) override;
    void post(const QString &url, const Headers &headers, const QByteArray &body, const QString &contentType,
               Callback callback) override;
    void patch(const QString &url, const Headers &headers, const QByteArray &body, const QString &contentType,
                Callback callback) override;

private:
    std::unique_ptr<QNetworkAccessManager> m_manager;
};
