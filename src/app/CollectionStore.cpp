#include "CollectionStore.h"

#include <QSettings>

#include <algorithm>

namespace {

constexpr const char *kNamesGroup = "CollectionNames";
constexpr const char *kMembershipGroup = "CollectionMembership";

struct Membership
{
    QString bookHash;
    QString collection;
};

QStringList readNames()
{
    QSettings settings;
    QStringList names;
    const int size = settings.beginReadArray(kNamesGroup);
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        const QString name = settings.value("name").toString();
        if (!name.isEmpty()) {
            names.append(name);
        }
    }
    settings.endArray();
    return names;
}

void writeNames(const QStringList &names)
{
    QSettings settings;
    settings.remove(kNamesGroup);
    settings.beginWriteArray(kNamesGroup);
    for (int i = 0; i < names.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("name", names[i]);
    }
    settings.endArray();
}

QVector<Membership> readMembership()
{
    QSettings settings;
    QVector<Membership> result;
    const int size = settings.beginReadArray(kMembershipGroup);
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        Membership m;
        m.bookHash = settings.value("bookHash").toString();
        m.collection = settings.value("collection").toString();
        if (!m.bookHash.isEmpty() && !m.collection.isEmpty()) {
            result.append(m);
        }
    }
    settings.endArray();
    return result;
}

void writeMembership(const QVector<Membership> &entries)
{
    QSettings settings;
    settings.remove(kMembershipGroup);
    settings.beginWriteArray(kMembershipGroup);
    for (int i = 0; i < entries.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("bookHash", entries[i].bookHash);
        settings.setValue("collection", entries[i].collection);
    }
    settings.endArray();
}

} // namespace

namespace CollectionStore {

QStringList allCollections()
{
    return readNames();
}

QString createCollection(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    QStringList names = readNames();
    if (!names.contains(trimmed)) {
        names.append(trimmed);
        writeNames(names);
    }
    return trimmed;
}

void renameCollection(const QString &oldName, const QString &newName)
{
    const QString trimmedNew = newName.trimmed();
    if (trimmedNew.isEmpty() || oldName == trimmedNew) {
        return;
    }

    QStringList names = readNames();
    if (!names.contains(oldName)) {
        return;
    }

    const bool targetExists = names.contains(trimmedNew);
    if (targetExists) {
        // Merging into an existing collection -- oldName just disappears,
        // rather than ending up with two entries of the same name.
        names.removeAll(oldName);
    } else {
        names.replace(names.indexOf(oldName), trimmedNew);
    }
    writeNames(names);

    QVector<Membership> membership = readMembership();
    for (Membership &m : membership) {
        if (m.collection == oldName) {
            m.collection = trimmedNew;
        }
    }
    // A book that was already in both oldName and trimmedNew would
    // otherwise end up with two now-identical membership rows -- collapse
    // those rather than leaving a harmless but wasteful duplicate.
    QVector<Membership> deduped;
    for (const Membership &m : membership) {
        bool alreadyPresent = false;
        for (const Membership &existing : deduped) {
            if (existing.bookHash == m.bookHash && existing.collection == m.collection) {
                alreadyPresent = true;
                break;
            }
        }
        if (!alreadyPresent) {
            deduped.append(m);
        }
    }
    writeMembership(deduped);
}

void deleteCollection(const QString &name)
{
    QStringList names = readNames();
    if (names.removeAll(name) > 0) {
        writeNames(names);
    }

    QVector<Membership> membership = readMembership();
    const int before = membership.size();
    membership.erase(std::remove_if(membership.begin(), membership.end(),
                                     [&](const Membership &m) { return m.collection == name; }),
                      membership.end());
    if (membership.size() != before) {
        writeMembership(membership);
    }
}

QStringList collectionsForBook(const QString &bookHash)
{
    QStringList result;
    for (const Membership &m : readMembership()) {
        if (m.bookHash == bookHash) {
            result.append(m.collection);
        }
    }
    return result;
}

QStringList booksInCollection(const QString &collection)
{
    QStringList result;
    for (const Membership &m : readMembership()) {
        if (m.collection == collection) {
            result.append(m.bookHash);
        }
    }
    return result;
}

void addBookToCollection(const QString &bookHash, const QString &collection)
{
    if (bookHash.isEmpty()) {
        return;
    }
    const QString name = createCollection(collection);
    if (name.isEmpty()) {
        return;
    }

    QVector<Membership> membership = readMembership();
    for (const Membership &m : membership) {
        if (m.bookHash == bookHash && m.collection == name) {
            return; // already a member
        }
    }
    membership.append({bookHash, name});
    writeMembership(membership);
}

void removeBookFromCollection(const QString &bookHash, const QString &collection)
{
    QVector<Membership> membership = readMembership();
    const int before = membership.size();
    membership.erase(std::remove_if(membership.begin(), membership.end(),
                                     [&](const Membership &m) {
                                         return m.bookHash == bookHash && m.collection == collection;
                                     }),
                      membership.end());
    if (membership.size() != before) {
        writeMembership(membership);
    }
}

} // namespace CollectionStore
