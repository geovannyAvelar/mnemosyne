#include "IHttpClient.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace {

QNetworkRequest buildRequest(const QString &url, const IHttpClient::Headers &headers)
{
    QNetworkRequest request{QUrl(url)};
    for (const auto &header : headers) {
        request.setRawHeader(header.first.toUtf8(), header.second.toUtf8());
    }
    return request;
}

void handleReply(QNetworkReply *reply, IHttpClient::Callback callback)
{
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, callback = std::move(callback)]() {
        IHttpClient::Response response;
        response.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        response.body = reply->readAll();
        if (response.statusCode == 0) {
            response.errorString = reply->errorString();
        }
        reply->deleteLater();
        callback(response);
    });
}

} // namespace

QtHttpClient::QtHttpClient() : m_manager(std::make_unique<QNetworkAccessManager>()) { }

QtHttpClient::~QtHttpClient() = default;

void QtHttpClient::get(const QString &url, const Headers &headers, Callback callback)
{
    QNetworkReply *reply = m_manager->get(buildRequest(url, headers));
    handleReply(reply, std::move(callback));
}

void QtHttpClient::post(const QString &url, const Headers &headers, const QByteArray &body,
                          const QString &contentType, Callback callback)
{
    QNetworkRequest request = buildRequest(url, headers);
    request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    QNetworkReply *reply = m_manager->post(request, body);
    handleReply(reply, std::move(callback));
}

void QtHttpClient::patch(const QString &url, const Headers &headers, const QByteArray &body,
                           const QString &contentType, Callback callback)
{
    QNetworkRequest request = buildRequest(url, headers);
    request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    QNetworkReply *reply = m_manager->sendCustomRequest(request, "PATCH", body);
    handleReply(reply, std::move(callback));
}
