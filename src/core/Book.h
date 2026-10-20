#pragma once

#include <QDateTime>
#include <QString>
#include <QVariantMap>

namespace stacks {

struct Book {
    int id = 0;
    QString title;
    QString author;
    QString format; // "pdf" or "epub"
    QString path;
    double progress = 0.0;
    QString locator;
    int pageCount = 0;
    int wordCount = 0;
    QString accent = "#C46A3B";
    QDateTime addedAt;
    QDateTime lastOpenedAt;

    QVariantMap toVariant() const
    {
        QVariantMap map;
        map.insert("id", id);
        map.insert("title", title);
        map.insert("author", author);
        map.insert("format", format);
        map.insert("path", path);
        map.insert("progress", progress);
        map.insert("locator", locator);
        map.insert("pageCount", pageCount);
        map.insert("wordCount", wordCount);
        map.insert("accent", accent);
        map.insert("addedAt", addedAt.toString(Qt::ISODate));
        map.insert("lastOpenedAt", lastOpenedAt.toString(Qt::ISODate));
        map.insert("hasStarted", progress > 0.0 || !locator.isEmpty());
        return map;
    }
};

struct Annotation {
    int id = 0;
    int bookId = 0;
    QString kind; // highlight, bookmark, note, voice
    QString locator;
    QString quote;
    QString note;
    QString color = "#C46A3B";
    int page = 0;
    int voiceId = 0;
    QString audioPath;
    int durationMs = 0;
    QDateTime createdAt;

    QVariantMap toVariant() const
    {
        QVariantMap map;
        map.insert("id", id);
        map.insert("bookId", bookId);
        map.insert("kind", kind);
        map.insert("locator", locator);
        map.insert("quote", quote);
        map.insert("note", note);
        map.insert("color", color);
        map.insert("page", page);
        map.insert("voiceId", voiceId);
        map.insert("audioPath", audioPath);
        map.insert("durationMs", durationMs);
        map.insert("createdAt", createdAt.toString(Qt::ISODate));
        return map;
    }
};

struct SearchResult {
    int passageId = 0;
    int bookId = 0;
    QString bookTitle;
    QString bookAuthor;
    QString bookFormat;
    QString title;
    QString snippet;
    QString locator;
    int page = 0;
    double rank = 0.0;

    QVariantMap toVariant() const
    {
        QVariantMap map;
        map.insert("passageId", passageId);
        map.insert("bookId", bookId);
        map.insert("bookTitle", bookTitle);
        map.insert("bookAuthor", bookAuthor);
        map.insert("format", bookFormat);
        map.insert("title", title);
        map.insert("snippet", snippet);
        map.insert("locator", locator);
        map.insert("page", page);
        map.insert("rank", rank);
        return map;
    }
};

struct DiagramNode {
    int id = 0;
    int bookId = 0;
    QString label;
    QString body;
    double x = 0;
    double y = 0;
    QDateTime createdAt;

    QVariantMap toVariant() const
    {
        QVariantMap map;
        map.insert("id", id);
        map.insert("bookId", bookId);
        map.insert("label", label);
        map.insert("body", body);
        map.insert("x", x);
        map.insert("y", y);
        map.insert("createdAt", createdAt.toString(Qt::ISODate));
        return map;
    }
};

struct DiagramEdge {
    int id = 0;
    int fromNode = 0;
    int toNode = 0;
    QString label;

    QVariantMap toVariant() const
    {
        QVariantMap map;
        map.insert("id", id);
        map.insert("fromId", fromNode);
        map.insert("toId", toNode);
        map.insert("label", label);
        return map;
    }
};

struct Passage {
    QString locator;
    QString text;
    int page = 0;
};

} // namespace stacks
