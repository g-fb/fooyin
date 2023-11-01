/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
 *
 * Fooyin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Fooyin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Fooyin.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <core/track.h>

#include <core/constants.h>
#include <core/corepaths.h>
#include <utils/utils.h>

#include <QIODevice>
#include <QMap>

using namespace Qt::Literals::StringLiterals;

constexpr auto Separator = "|";

namespace Fy::Core {
struct Track::Private : public QSharedData
{
    int libraryId{-1};
    bool enabled{true};
    int id{-1};
    QString hash;
    QString filepath;
    QString relativePath;
    QString title;
    QStringList artists;
    QString album;
    QString albumArtist;
    int trackNumber{-1};
    int trackTotal{-1};
    int discNumber{-1};
    int discTotal{-1};
    QStringList genres;
    QString composer;
    QString performer;
    uint64_t duration{0};
    QString lyrics;
    QString comment;
    QString date;
    int year{-1};
    QString coverPath;
    ExtraTags extraTags;

    uint64_t filesize{0};
    int bitrate{0};
    int sampleRate{0};

    int playcount{0};

    uint64_t addedTime{0};
    uint64_t modifiedTime{0};

    QString sort;

    explicit Private(QString filepath = {})
        : filepath{std::move(filepath)}
    { }
};

Track::Track()
    : Track{u""_s}
{ }

Track::Track(QString filepath)
    : p{new Private(std::move(filepath))}
{ }

bool Track::operator==(const Track& other) const
{
    return filepath() == other.filepath() && hash() == other.hash();
}

bool Track::operator!=(const Track& other) const
{
    return filepath() != other.filepath() && hash() != other.hash();
}

Track::~Track()                             = default;
Track::Track(const Track& other)            = default;
Track& Track::operator=(const Track& other) = default;

QString Track::generateHash()
{
    p->hash = p->artists.join(","_L1) + Separator + p->album + Separator + p->date + Separator
            + QString::number(p->discNumber) + Separator + Utils::addLeadingZero(p->trackNumber, 2) + Separator
            + QString::number(p->duration);
    return p->hash;
}

bool Track::isValid() const
{
    return p->id >= 0 && !p->filepath.isEmpty();
}

bool Track::enabled() const
{
    return p->enabled;
}

int Track::libraryId() const
{
    return p->libraryId;
}

int Track::id() const
{
    return p->id;
}

QString Track::hash() const
{
    return p->hash;
}

QString Track::albumHash() const
{
    return u"%1|%2|%3"_s.arg(p->date, !p->albumArtist.isEmpty() ? p->albumArtist : p->artists.join(""_L1), p->album);
}

QString Track::filepath() const
{
    return p->filepath;
}

QString Track::relativePath() const
{
    return p->relativePath;
}

QString Track::title() const
{
    return p->title;
}

QStringList Track::artists() const
{
    return p->artists;
}

QString Track::artist() const
{
    return p->artists.join(Constants::Separator);
}

QString Track::album() const
{
    return p->album;
}

QString Track::albumArtist() const
{
    return p->albumArtist;
}

int Track::trackNumber() const
{
    return p->trackNumber;
}

int Track::trackTotal() const
{
    return p->trackTotal;
}

int Track::discNumber() const
{
    return p->discNumber;
}

int Track::discTotal() const
{
    return p->discTotal;
}

QStringList Track::genres() const
{
    return p->genres;
}

QString Track::genre() const
{
    return p->genres.join(Constants::Separator);
}

QString Track::composer() const
{
    return p->composer;
}

QString Track::performer() const
{
    return p->performer;
}

uint64_t Track::duration() const
{
    return p->duration;
}

QString Track::lyrics() const
{
    return p->lyrics;
}

QString Track::comment() const
{
    return p->comment;
}

QString Track::date() const
{
    return p->date;
}

int Track::year() const
{
    return p->year;
}

QString Track::coverPath() const
{
    return p->coverPath;
}

bool Track::hasCover() const
{
    return !p->coverPath.isEmpty();
}

bool Track::hasEmbeddedCover() const
{
    return p->coverPath == Constants::EmbeddedCover;
}

ExtraTags Track::extraTags() const
{
    return p->extraTags;
}

QByteArray Track::serialiseExtrasTags() const
{
    if(p->extraTags.empty()) {
        return {};
    }

    QByteArray tags;
    QDataStream out(&tags, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_5);

    out << p->extraTags;

    return tags;
}

uint64_t Track::fileSize() const
{
    return p->filesize;
}

int Track::bitrate() const
{
    return p->bitrate;
}

int Track::sampleRate() const
{
    return p->sampleRate;
}

int Track::playCount() const
{
    return p->playcount;
}

uint64_t Track::addedTime() const
{
    return p->addedTime;
}

uint64_t Track::modifiedTime() const
{
    return p->modifiedTime;
}

QString Track::sort() const
{
    return p->sort;
}

void Track::setLibraryId(int id)
{
    p->libraryId = id;
}

void Track::setEnabled(bool enabled)
{
    p->enabled = enabled;
}

void Track::setId(int id)
{
    p->id = id;
}

void Track::setHash(const QString& hash)
{
    p->hash = hash;
}

void Track::setRelativePath(const QString& path)
{
    p->relativePath = path;
}

void Track::setTitle(const QString& title)
{
    p->title = title;
}

void Track::setArtists(const QStringList& artists)
{
    p->artists = artists;
}

void Track::setAlbum(const QString& title)
{
    p->album = title;
}

void Track::setAlbumArtist(const QString& artist)
{
    p->albumArtist = artist;
}

void Track::setTrackNumber(int number)
{
    p->trackNumber = number;
}

void Track::setTrackTotal(int total)
{
    p->trackTotal = total;
}

void Track::setDiscNumber(int number)
{
    p->discNumber = number;
}

void Track::setDiscTotal(int total)
{
    p->discTotal = total;
}

void Track::setGenres(const QStringList& genres)
{
    p->genres = genres;
}

void Track::setComposer(const QString& composer)
{
    p->composer = composer;
}

void Track::setPerformer(const QString& performer)
{
    p->performer = performer;
}

void Track::setDuration(uint64_t duration)
{
    p->duration = duration;
}

void Track::setLyrics(const QString& lyrics)
{
    p->lyrics = lyrics;
}

void Track::setComment(const QString& comment)
{
    p->comment = comment;
}

void Track::setDate(const QString& date)
{
    p->date = date;
}

void Track::setYear(int year)
{
    p->year = year;
}

void Track::setCoverPath(const QString& path)
{
    p->coverPath = path;
}

void Track::addExtraTag(const QString& tag, const QString& value)
{
    if(tag.isEmpty() || value.isEmpty()) {
        return;
    }
    if(p->extraTags.contains(tag)) {
        auto entry = p->extraTags.value(tag);
        entry.emplace_back(value);
        p->extraTags.insert(tag, entry);
    }
    else {
        p->extraTags.insert(tag, {value});
    }
}

void Track::storeExtraTags(const QByteArray& tags)
{
    if(!tags.isEmpty()) {
        QByteArray tagsArray{tags};

        QDataStream in(&tagsArray, QIODevice::ReadOnly);
        in.setVersion(QDataStream::Qt_6_5);

        in >> p->extraTags;
    }
}

void Track::setFileSize(uint64_t fileSize)
{
    p->filesize = fileSize;
}

void Track::setBitrate(int rate)
{
    p->bitrate = rate;
}

void Track::setSampleRate(int rate)
{
    p->sampleRate = rate;
}

void Track::setPlayCount(int count)
{
    p->playcount = count;
}

void Track::setAddedTime(uint64_t time)
{
    p->addedTime = time;
}

void Track::setModifiedTime(uint64_t time)
{
    p->modifiedTime = time;
}

void Track::setSort(const QString& sort)
{
    p->sort = sort;
}

size_t qHash(const Track& track)
{
    return qHash(track.filepath());
}
} // namespace Fy::Core