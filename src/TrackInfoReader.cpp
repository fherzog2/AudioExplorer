// SPDX-License-Identifier: GPL-2.0-only
#include "TrackInfoReader.h"

#include <taglib/tstring.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/mpegfile.h>
#include <taglib/vorbisfile.h>
#include <taglib/mpcfile.h>
#include <taglib/mp4file.h>
#include <taglib/wavfile.h>
#include <taglib/asffile.h>
#include <taglib/apefile.h>
#include <taglib/flacfile.h>
#include <taglib/oggflacfile.h>
#include <taglib/speexfile.h>
#include <taglib/opusfile.h>
#include <taglib/aifffile.h>
#include <taglib/itfile.h>
#include <taglib/modfile.h>
#include <taglib/s3mfile.h>
#include <taglib/xmfile.h>
#include <taglib/trueaudiofile.h>
#include <taglib/wavpackfile.h>
#include <taglib/id3v1tag.h>
#include <taglib/id3v2tag.h>
#include <taglib/apetag.h>
#include <taglib/attachedpictureframe.h>

namespace{
    QString toQString(const TagLib::String& s)
    {
        return QString::fromUtf8(s.data(TagLib::String::UTF8).data());
    }

    void readBasicTrackInfo(TagLib::Tag* tag, TrackInfo& info)
    {
        info.artist = toQString(tag->artist());
        info.album = toQString(tag->album());
        info.year = tag->year();
        info.genre = toQString(tag->genre());
        info.title = toQString(tag->title());
        info.track_number = tag->track();
        info.comment = toQString(tag->comment());
    }

    void appendTagType(const QString& type, TrackInfo& info)
    {
        if (!info.tag_types.isEmpty())
            info.tag_types += ", ";
        info.tag_types += type;
    }

    void readID3v1Info(TrackInfo& info)
    {
        appendTagType("ID3v1", info);
    }

    void readID3v2Info(TagLib::ID3v2::Tag* tag, TrackInfo& info)
    {
        appendTagType("ID3v2", info);

        if (info.cover.isEmpty())
        {
            TagLib::ID3v2::FrameList picture_frames = tag->frameListMap()["APIC"];

            const TagLib::ID3v2::AttachedPictureFrame* first_picture_frame = nullptr;
            const TagLib::ID3v2::AttachedPictureFrame* picture_frame = nullptr;

            for (TagLib::ID3v2::Frame* frame : picture_frames)
                if (auto f = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame*>(frame))
                {
                    if (!first_picture_frame)
                        first_picture_frame = f;

                    if (f->type() == TagLib::ID3v2::AttachedPictureFrame::FrontCover)
                    {
                        picture_frame = f;
                        break;
                    }
                }

            if (!picture_frame)
                picture_frame = first_picture_frame;

            if (picture_frame)
            {
                TagLib::ByteVector bytes = picture_frame->picture();
                info.cover = QByteArray(bytes.data(), bytes.size());
            }
        }

        if (info.album_artist.isEmpty())
        {
            const TagLib::ID3v2::FrameListMap& frame_list_map = tag->frameListMap();

            auto it = frame_list_map.find("TPE2");
            if (it != frame_list_map.end() && !it->second.isEmpty())
            {
                info.album_artist = toQString(it->second.front()->toString());
            }
        }

        if (info.disc_number == 0)
        {
            const TagLib::ID3v2::FrameListMap& frame_list_map = tag->frameListMap();

            auto it = frame_list_map.find("TPOS");
            if (it != frame_list_map.end() && !it->second.isEmpty())
            {
                info.disc_number = it->second.front()->toString().toInt();
            }
        }
    }

    void readXiphCommentInfo(TagLib::Ogg::XiphComment* tag, TrackInfo& info)
    {
        appendTagType("Vorbis comment", info);

        if (info.cover.isEmpty())
        {
            TagLib::FLAC::Picture* first_picture = nullptr;
            TagLib::FLAC::Picture* picture = nullptr;

            for (TagLib::FLAC::Picture* p : tag->pictureList())
            {
                if (!first_picture)
                    first_picture = p;

                if (p->type() == TagLib::FLAC::Picture::FrontCover)
                {
                    picture = p;
                    break;
                }
            }

            if (!picture)
                picture = first_picture;

            if (picture)
            {
                TagLib::ByteVector bytes = picture->data();
                info.cover = QByteArray(bytes.data(), bytes.size());
            }
        }

        if (info.album_artist.isEmpty())
        {
            const TagLib::Ogg::FieldListMap& field_list_map = tag->fieldListMap();

            auto album_artists = field_list_map.find("ALBUMARTIST");
            if (album_artists != field_list_map.end())
            {
                if (!album_artists->second.isEmpty())
                {
                    info.album_artist = toQString(album_artists->second.front());
                }
            }
        }

        if (info.disc_number == 0)
        {
            const TagLib::Ogg::FieldListMap& field_list_map = tag->fieldListMap();

            auto it = field_list_map.find("DISCNUMBER");
            if (it != field_list_map.end() && !it->second.isEmpty())
            {
                info.disc_number = it->second.front().toInt();
            }
        }
    }

    void readMP4Info(TagLib::MP4::Tag* tag, TrackInfo& info)
    {
        appendTagType("MP4", info);

        if (info.cover.isEmpty())
        {
            const TagLib::MP4::ItemListMap& item_list_map = tag->itemListMap();
            if (item_list_map.contains("covr"))
            {
                const TagLib::MP4::Item& cover_item = item_list_map["covr"];
                TagLib::MP4::CoverArtList cover_art_list = cover_item.toCoverArtList();
                if (!cover_art_list.isEmpty())
                {
                    TagLib::ByteVector bytes = cover_art_list.front().data();
                    info.cover = QByteArray(bytes.data(), bytes.size());
                }
            }
        }

        if (info.album_artist.isEmpty())
        {
            const TagLib::MP4::ItemListMap& item_list_map = tag->itemListMap();

            auto album_artist_item = item_list_map.find("aART");
            if (album_artist_item != item_list_map.end())
            {
                TagLib::StringList album_artists = album_artist_item->second.toStringList();
                if (!album_artists.isEmpty())
                {
                    info.album_artist = toQString(album_artists.front());
                }
            }
        }

        if (info.disc_number == 0)
        {
            const TagLib::MP4::ItemListMap& item_list_map = tag->itemListMap();

            auto disc_number_item = item_list_map.find("disk");
            if (disc_number_item != item_list_map.end())
            {
                info.disc_number = disc_number_item->second.toInt();
            }
        }
    }

    void readAPEInfo(TagLib::APE::Tag* tag, TrackInfo& info)
    {
        appendTagType("APE", info);

        if (info.cover.isEmpty())
        {
            const TagLib::APE::ItemListMap& item_map = tag->itemListMap();
            if (item_map.contains("COVER ART (FRONT)"))
            {
                const TagLib::ByteVector null_terminator(1, 0);
                TagLib::ByteVector item = item_map["COVER ART (FRONT)"].value();
                const int pos = item.find(null_terminator); // Skip the filename.
                if (pos != -1)
                {
                    TagLib::ByteVector bytes = item.mid(pos + 1);
                    info.cover = QByteArray(bytes.data(), bytes.size());
                }
            }
        }

        if (info.album_artist.isEmpty())
        {
            const TagLib::APE::ItemListMap& item_map = tag->itemListMap();

            auto album_artist = item_map.find("ALBUMARTIST");
            if (album_artist != item_map.end())
            {
                info.album_artist = toQString(album_artist->second.toString());
            }
        }

        if (info.disc_number == 0)
        {
            const TagLib::APE::ItemListMap& item_map = tag->itemListMap();

            auto it = item_map.find("DISCNUMBER");
            if (it != item_map.end())
            {
                info.disc_number = it->second.toString().toInt();
            }
        }
    }

    void readASFInfo(TagLib::ASF::Tag* tag, TrackInfo& info)
    {
        appendTagType("ASF", info);

        if (info.cover.isEmpty())
        {
            const TagLib::ASF::AttributeListMap& attr_list_map = tag->attributeListMap();
            if (attr_list_map.contains("WM/Picture"))
            {
                const TagLib::ASF::AttributeList& attr_list = attr_list_map["WM/Picture"];
                if (!attr_list.isEmpty())
                {
                    auto found = std::find_if(attr_list.begin(), attr_list.end(), [](const TagLib::ASF::Attribute& a){
                        return a.toPicture().type() == TagLib::ASF::Picture::FrontCover;
                    });

                    if (found == attr_list.end())
                        found = attr_list.begin();

                    TagLib::ByteVector bytes = found->toPicture().picture();
                    info.cover = QByteArray(bytes.data(), bytes.size());
                }
            }
        }

        if (info.album_artist.isEmpty())
        {
            const TagLib::ASF::AttributeListMap& attr_list_map = tag->attributeListMap();

            auto album_artists = attr_list_map.find("WM/AlbumArtist");
            if (album_artists != attr_list_map.end())
            {
                if (!album_artists->second.isEmpty())
                {
                    info.album_artist = toQString(album_artists->second.front().toString());
                }
            }
        }

        if (info.disc_number == 0)
        {
            const TagLib::ASF::AttributeListMap& attr_list_map = tag->attributeListMap();

            auto it = attr_list_map.find("WM/PartOfSet");
            if (it != attr_list_map.end() && !it->second.isEmpty())
            {
                info.disc_number = it->second.front().toString().toInt();
            }
        }
    }

    void readModInfo(TrackInfo& info)
    {
        appendTagType("MOD", info);
    }

    void readInfoInfo(TrackInfo& info)
    {
        appendTagType("Info", info);
    }
}

bool readTrackInfo(const QString& filepath, TrackInfo& info)
{
    {
        // TagLib::FileName is a different type on Windows
#if _WIN32
        TagLib::FileRef file_ref(TagLib::FileName(filepath.toStdWString().data()));
#else
        TagLib::FileRef file_ref(TagLib::FileName(filepath.toStdString().data()));
#endif

        if (file_ref.tag())
        {
            readBasicTrackInfo(file_ref.tag(), info);

            info.length_milliseconds = file_ref.audioProperties()->lengthInMilliseconds();
            info.channels            = file_ref.audioProperties()->channels();
            info.bitrate_kbs         = file_ref.audioProperties()->bitrate();
            info.samplerate_hz       = file_ref.audioProperties()->sampleRate();

            if (TagLib::MPEG::File* file = dynamic_cast<TagLib::MPEG::File*>(file_ref.file()))
            {
                if (file->hasID3v1Tag())
                    readID3v1Info(info);
                if (file->hasID3v2Tag())
                    readID3v2Info(file->ID3v2Tag(), info);
                if (file->hasAPETag())
                    readAPEInfo(file->APETag(), info);
            }
            if (TagLib::Ogg::Vorbis::File* file = dynamic_cast<TagLib::Ogg::Vorbis::File*>(file_ref.file()))
            {
                readXiphCommentInfo(file->tag(), info);
            }
            if (TagLib::Ogg::Opus::File* file = dynamic_cast<TagLib::Ogg::Opus::File*>(file_ref.file()))
            {
                readXiphCommentInfo(file->tag(), info);
            }
            if (TagLib::Ogg::FLAC::File* file = dynamic_cast<TagLib::Ogg::FLAC::File*>(file_ref.file()))
            {
                readXiphCommentInfo(file->tag(), info);
            }
            if (TagLib::Ogg::Speex::File* file = dynamic_cast<TagLib::Ogg::Speex::File*>(file_ref.file()))
            {
                readXiphCommentInfo(file->tag(), info);
            }
            if (TagLib::RIFF::WAV::File* file = dynamic_cast<TagLib::RIFF::WAV::File*>(file_ref.file()))
            {
                if (file->hasID3v2Tag())
                    readID3v2Info(file->ID3v2Tag(), info);
                if (file->hasInfoTag())
                    readInfoInfo(info);
            }
            if (TagLib::RIFF::AIFF::File* file = dynamic_cast<TagLib::RIFF::AIFF::File*>(file_ref.file()))
            {
                if (file->hasID3v2Tag())
                    readID3v2Info(file->tag(), info);
            }
            if (TagLib::MPC::File* file = dynamic_cast<TagLib::MPC::File*>(file_ref.file()))
            {
                if (file->hasID3v1Tag())
                    readID3v1Info(info);
                if (file->hasAPETag())
                    readAPEInfo(file->APETag(), info);
            }
            if (TagLib::MP4::File* file = dynamic_cast<TagLib::MP4::File*>(file_ref.file()))
            {
                if (file->hasMP4Tag())
                    readMP4Info(file->tag(), info);
            }
            if (TagLib::FLAC::File* file = dynamic_cast<TagLib::FLAC::File*>(file_ref.file()))
            {
                if (file->hasID3v1Tag())
                    readID3v1Info(info);
                if (file->hasID3v2Tag())
                    readID3v2Info(file->ID3v2Tag(), info);
                if (file->hasXiphComment())
                    readXiphCommentInfo(file->xiphComment(), info);
            }
            if (TagLib::ASF::File* file = dynamic_cast<TagLib::ASF::File*>(file_ref.file()))
            {
                readASFInfo(file->tag(), info);
            }
            if (TagLib::APE::File* file = dynamic_cast<TagLib::APE::File*>(file_ref.file()))
            {
                if (file->hasID3v1Tag())
                    readID3v1Info(info);
                if (file->hasAPETag())
                    readAPEInfo(file->APETag(), info);
            }
            if (dynamic_cast<TagLib::IT::File*>(file_ref.file()))
            {
                readModInfo(info);
            }
            if (dynamic_cast<TagLib::Mod::File*>(file_ref.file()))
            {
                readModInfo(info);
            }
            if (dynamic_cast<TagLib::S3M::File*>(file_ref.file()))
            {
                readModInfo(info);
            }
            if (dynamic_cast<TagLib::XM::File*>(file_ref.file()))
            {
                readModInfo(info);
            }
            if (TagLib::TrueAudio::File* file = dynamic_cast<TagLib::TrueAudio::File*>(file_ref.file()))
            {
                if (file->hasID3v1Tag())
                    readID3v1Info(info);
                if (file->hasID3v2Tag())
                    readID3v2Info(file->ID3v2Tag(), info);
            }
            if (TagLib::WavPack::File* file = dynamic_cast<TagLib::WavPack::File*>(file_ref.file()))
            {
                if (file->hasID3v1Tag())
                    readID3v1Info(info);
                if (file->hasAPETag())
                    readAPEInfo(file->APETag(), info);
            }

            return true;
        }
    }

    return false;
}