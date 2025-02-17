/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "playlistdatabase.h"

#include <utils/database/dbquery.h>
#include <utils/database/dbtransaction.h>

using namespace Qt::StringLiterals;

namespace Fooyin {
std::vector<PlaylistInfo> PlaylistDatabase::getAllPlaylists()
{
    const QString query
        = u"SELECT PlaylistID, Name, PlaylistIndex, IsAutoPlaylist, Query FROM Playlists ORDER BY PlaylistIndex;"_s;

    DbQuery q{db(), query};

    if(!q.exec()) {
        return {};
    }

    std::vector<PlaylistInfo> playlists;

    while(q.next()) {
        PlaylistInfo playlist;

        playlist.dbId           = q.value(0).toInt();
        playlist.name           = q.value(1).toString();
        playlist.index          = q.value(2).toInt();
        playlist.isAutoPlaylist = q.value(3).toBool();
        playlist.query          = q.value(4).toString();

        playlists.emplace_back(playlist);
    }

    return playlists;
}

TrackList PlaylistDatabase::getPlaylistTracks(const Playlist& playlist, const std::unordered_map<int, Track>& tracks)
{
    return populatePlaylistTracks(playlist, tracks);
}

int PlaylistDatabase::insertPlaylist(const QString& name, int index, bool isAutoPlaylist, const QString& autoQuery)
{
    if(name.isEmpty() || index < 0) {
        return -1;
    }

    const QString statement = u"INSERT INTO Playlists (Name, PlaylistIndex, IsAutoPlaylist, Query) "
                              "VALUES (:name, :index, :isAutoPlaylist, :query);"_s;

    DbQuery query{db(), statement};
    query.bindValue(u":name"_s, name);
    query.bindValue(u":index"_s, index);
    query.bindValue(u":isAutoPlaylist"_s, isAutoPlaylist);
    query.bindValue(u":query"_s, autoQuery);

    if(!query.exec()) {
        return -1;
    }

    return query.lastInsertId().toInt();
}

bool PlaylistDatabase::savePlaylist(Playlist& playlist)
{
    bool updated{false};

    if(playlist.modified()) {
        const auto statement = u"UPDATE Playlists SET Name = :name, PlaylistIndex = :index, IsAutoPlaylist = "
                               ":isAutoPlaylist, Query = :query WHERE PlaylistID = :id;"_s;

        DbQuery query{db(), statement};

        query.bindValue(u":name"_s, playlist.name());
        query.bindValue(u":index"_s, playlist.index());
        query.bindValue(u":isAutoPlaylist"_s, playlist.isAutoPlaylist());
        query.bindValue(u":query"_s, playlist.query());
        query.bindValue(u":id"_s, playlist.dbId());

        updated = query.exec();
    }

    if(!playlist.isAutoPlaylist() && playlist.tracksModified()) {
        updated = insertPlaylistTracks(playlist.dbId(), playlist.tracks());
    }

    if(updated) {
        playlist.resetFlags();
        return true;
    }

    return false;
}

bool PlaylistDatabase::saveModifiedPlaylists(const PlaylistList& playlists)
{
    DbTransaction transaction{db()};

    for(const auto& playlist : playlists) {
        savePlaylist(*playlist);
    }

    return transaction.commit();
}

bool PlaylistDatabase::removePlaylist(int id)
{
    const auto statement = u"DELETE FROM Playlists WHERE PlaylistID = :id;"_s;

    DbQuery query{db(), statement};
    query.bindValue(u":id"_s, id);

    return query.exec();
}

bool PlaylistDatabase::renamePlaylist(int id, const QString& name)
{
    if(name.isEmpty()) {
        return false;
    }

    const auto statement = u"UPDATE Playlists SET Name = :name WHERE PlaylistID = :id;"_s;

    DbQuery query{db(), statement};
    query.bindValue(u":name"_s, name);
    query.bindValue(u":id"_s, id);

    return query.exec();
}

bool PlaylistDatabase::insertPlaylistTrack(int playlistId, const Track& track, int index)
{
    const QString statement
        = u"INSERT INTO PlaylistTracks (PlaylistID, TrackID, TrackIndex) VALUES (:playlistId, :trackId, :index);"_s;

    DbQuery query{db(), statement};
    query.bindValue(u":playlistId"_s, playlistId);
    query.bindValue(u":trackId"_s, track.id());
    query.bindValue(u":index"_s, index);

    return query.exec();
}

bool PlaylistDatabase::insertPlaylistTracks(int playlistId, const TrackList& tracks)
{
    if(playlistId < 0) {
        return false;
    }

    // Remove current playlist tracks
    const auto statement = u"DELETE FROM PlaylistTracks WHERE PlaylistID = :id;"_s;

    DbQuery query{db(), statement};
    query.bindValue(u":id"_s, playlistId);

    if(!query.exec()) {
        return false;
    }

    for(int i{0}; const auto& track : tracks) {
        if(track.isValid() && track.isInDatabase()) {
            if(!insertPlaylistTrack(playlistId, track, i++)) {
                return false;
            }
        }
    }

    return true;
}

TrackList PlaylistDatabase::populatePlaylistTracks(const Playlist& playlist,
                                                   const std::unordered_map<int, Track>& tracks)
{
    const auto statement = u"SELECT TrackID FROM PlaylistTracks WHERE PlaylistID=:playlistId ORDER BY TrackIndex;"_s;

    DbQuery query{db(), statement};
    query.bindValue(u":playlistId"_s, playlist.dbId());

    if(!query.exec()) {
        return {};
    }

    TrackList playlistTracks;

    while(query.next()) {
        const int trackId = query.value(0).toInt();
        if(tracks.contains(trackId)) {
            playlistTracks.push_back(tracks.at(trackId));
        }
    }

    return playlistTracks;
}
} // namespace Fooyin
