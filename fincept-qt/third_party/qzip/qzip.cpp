// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qzipreader_p.h"
#include "qzipwriter_p.h"

#include <qdatetime.h>
#include <qendian.h>
#include <qdebug.h>
#include <qdir.h>

#include <memory>

#include <zlib.h>

// Zip standard version for archives handled by this API
// (actually, the only basic support of this version is implemented but it is enough for now)
#define ZIP_VERSION 20

#if 0
#define ZDEBUG qDebug
#else
#define ZDEBUG if (0) qDebug
#endif

QT_BEGIN_NAMESPACE

static inline uint readUInt(const uchar *data)
{
    return (data[0]) + (data[1]<<8) + (data[2]<<16) + (data[3]<<24);
}

static inline ushort readUShort(const uchar *data)
{
    return (data[0]) + (data[1]<<8);
}

static inline void writeUInt(uchar *data, uint i)
{
    data[0] = i & 0xff;
    data[1] = (i>>8) & 0xff;
    data[2] = (i>>16) & 0xff;
    data[3] = (i>>24) & 0xff;
}

static inline void writeUShort(uchar *data, ushort i)
{
    data[0] = i & 0xff;
    data[1] = (i>>8) & 0xff;
}

static inline void copyUInt(uchar *dest, const uchar *src)
{
    dest[0] = src[0];
    dest[1] = src[1];
    dest[2] = src[2];
    dest[3] = src[3];
}

static inline void copyUShort(uchar *dest, const uchar *src)
{
    dest[0] = src[0];
    dest[1] = src[1];
}

static void writeMSDosDate(uchar *dest, const QDateTime& dt)
{
    if (dt.isValid()) {
        quint16 time =
            (dt.time().hour() << 11)    // 5 bit hour
            | (dt.time().minute() << 5)   // 6 bit minute
            | (dt.time().second() >> 1);  // 5 bit double seconds

        dest[0] = time & 0xff;
        dest[1] = time >> 8;

        quint16 date =
            ((dt.date().year() - 1980) << 9) // 7 bit year 1980-based
            | (dt.date().month() << 5)           // 4 bit month
            | (dt.date().day());                 // 5 bit day

        dest[2] = char(date);
        dest[3] = char(date >> 8);
    } else {
        dest[0] = 0;
        dest[1] = 0;
        dest[2] = 0;
        dest[3] = 0;
    }
}

static int inflate(Bytef *dest, ulong *destLen, const Bytef *source, ulong sourceLen)
{
    z_stream stream = {};
    int err;

    stream.next_in = const_cast<Bytef*>(source);
    stream.avail_in = (uInt)sourceLen;
    if ((uLong)stream.avail_in != sourceLen)
        return Z_BUF_ERROR;

    stream.next_out = dest;
    stream.avail_out = (uInt)*destLen;
    if ((uLong)stream.avail_out != *destLen)
        return Z_BUF_ERROR;

    err = inflateInit2(&stream, -MAX_WBITS);
    if (err != Z_OK)
        return err;

    err = inflate(&stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        inflateEnd(&stream);
        if (err == Z_NEED_DICT || (err == Z_BUF_ERROR && stream.avail_in == 0))
            return Z_DATA_ERROR;
        return err;
    }
    *destLen = stream.total_out;

    err = inflateEnd(&stream);
    return err;
}

static int deflate (Bytef *dest, ulong *destLen, const Bytef *source, ulong sourceLen)
{
    z_stream stream;
    int err;

    stream.next_in = const_cast<Bytef*>(source);
    stream.avail_in = (uInt)sourceLen;
    stream.next_out = dest;
    stream.avail_out = (uInt)*destLen;
    if ((uLong)stream.avail_out != *destLen) return Z_BUF_ERROR;

    stream.zalloc = (alloc_func)nullptr;
    stream.zfree = (free_func)nullptr;
    stream.opaque = (voidpf)nullptr;

    err = deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    if (err != Z_OK) return err;

    err = deflate(&stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        deflateEnd(&stream);
        return err == Z_OK ? Z_BUF_ERROR : err;
    }
    *destLen = stream.total_out;

    err = deflateEnd(&stream);
    return err;
}


namespace WindowsFileAttributes {
enum {
    Dir        = 0x10, // FILE_ATTRIBUTE_DIRECTORY
    File       = 0x80, // FILE_ATTRIBUTE_NORMAL
    TypeMask   = 0x90,

    ReadOnly   = 0x01, // FILE_ATTRIBUTE_READONLY
    PermMask   = 0x01
};
}

namespace UnixFileAttributes {
enum {
    Dir        = 0040000, // __S_IFDIR
    File       = 0100000, // __S_IFREG
    SymLink    = 0120000, // __S_IFLNK
    TypeMask   = 0170000, // __S_IFMT

    ReadUser   = 0400, // __S_IRUSR
    WriteUser  = 0200, // __S_IWUSR
    ExeUser    = 0100, // __S_IXUSR
    ReadGroup  = 0040, // __S_IRGRP
    WriteGroup = 0020, // __S_IWGRP
    ExeGroup   = 0010, // __S_IXGRP
    ReadOther  = 0004, // __S_IROTH
    WriteOther = 0002, // __S_IWOTH
    ExeOther   = 0001, // __S_IXOTH
    PermMask   = 0777
};
}

static QFile::Permissions modeToPermissions(quint32 mode)
{
    QFile::Permissions ret;
    if (mode & UnixFileAttributes::ReadUser)
        ret |= QFile::ReadOwner | QFile::ReadUser;
    if (mode & UnixFileAttributes::WriteUser)
        ret |= QFile::WriteOwner | QFile::WriteUser;
    if (mode & UnixFileAttributes::ExeUser)
        ret |= QFile::ExeOwner | QFile::ExeUser;
    if (mode & UnixFileAttributes::ReadGroup)
        ret |= QFile::ReadGroup;
    if (mode & UnixFileAttributes::WriteGroup)
        ret |= QFile::WriteGroup;
    if (mode & UnixFileAttributes::ExeGroup)
        ret |= QFile::ExeGroup;
    if (mode & UnixFileAttributes::ReadOther)
        ret |= QFile::ReadOther;
    if (mode & UnixFileAttributes::WriteOther)
        ret |= QFile::WriteOther;
    if (mode & UnixFileAttributes::ExeOther)
        ret |= QFile::ExeOther;
    return ret;
}

static quint32 permissionsToMode(QFile::Permissions perms)
{
    quint32 mode = 0;
    if (perms & (QFile::ReadOwner | QFile::ReadUser))
        mode |= UnixFileAttributes::ReadUser;
    if (perms & (QFile::WriteOwner | QFile::WriteUser))
        mode |= UnixFileAttributes::WriteUser;
    if (perms & (QFile::ExeOwner | QFile::ExeUser))
        mode |= UnixFileAttributes::WriteUser;
    if (perms & QFile::ReadGroup)
        mode |= UnixFileAttributes::ReadGroup;
    if (perms & QFile::WriteGroup)
        mode |= UnixFileAttributes::WriteGroup;
    if (perms & QFile::ExeGroup)
        mode |= UnixFileAttributes::ExeGroup;
    if (perms & QFile::ReadOther)
        mode |= UnixFileAttributes::ReadOther;
    if (perms & QFile::WriteOther)
        mode |= UnixFileAttributes::WriteOther;
    if (perms & QFile::ExeOther)
        mode |= UnixFileAttributes::ExeOther;
    return mode;
}

static QDateTime readMSDosDate(const uchar *src)
{
    uint dosDate = readUInt(src);
    quint64 uDate;
    uDate = (quint64)(dosDate >> 16);
    uint tm_mday = (uDate & 0x1f);
    uint tm_mon =  ((uDate & 0x1E0) >> 5);
    uint tm_year = (((uDate & 0x0FE00) >> 9) + 1980);
    uint tm_hour = ((dosDate & 0xF800) >> 11);
    uint tm_min =  ((dosDate & 0x7E0) >> 5);
    uint tm_sec =  ((dosDate & 0x1f) << 1);

    return QDateTime(QDate(tm_year, tm_mon, tm_mday), QTime(tm_hour, tm_min, tm_sec));
}

// for details, see http://www.pkware.com/documents/casestudies/APPNOTE.TXT

enum HostOS {
    HostFAT      = 0,
    HostAMIGA    = 1,
    HostVMS      = 2,  // VAX/VMS
    HostUnix     = 3,
    HostVM_CMS   = 4,
    HostAtari    = 5,  // what if it's a minix filesystem? [cjh]
    HostHPFS     = 6,  // filesystem used by OS/2 (and NT 3.x)
    HostMac      = 7,
    HostZ_System = 8,
    HostCPM      = 9,
    HostTOPS20   = 10, // pkzip 2.50 NTFS
    HostNTFS     = 11, // filesystem used by Windows NT
    HostQDOS     = 12, // SMS/QDOS
    HostAcorn    = 13, // Archimedes Acorn RISC OS
    HostVFAT     = 14, // filesystem used by Windows 95, NT
    HostMVS      = 15,
    HostBeOS     = 16, // hybrid POSIX/database filesystem
    HostTandem   = 17,
    HostOS400    = 18,
    HostOSX      = 19
};
Q_DECLARE_TYPEINFO(HostOS, Q_PRIMITIVE_TYPE);

enum GeneralPurposeFlag {
    Encrypted = 0x01,
    AlgTune1 = 0x02,
    AlgTune2 = 0x04,
    HasDataDescriptor = 0x08,
    PatchedData = 0x20,
    StrongEncrypted = 0x40,
    Utf8Names = 0x0800,
    CentralDirectoryEncrypted = 0x2000
};
Q_DECLARE_TYPEINFO(GeneralPurposeFlag, Q_PRIMITIVE_TYPE);

enum CompressionMethod {
    CompressionMethodStored = 0,
    CompressionMethodShrunk = 1,
    CompressionMethodReduced1 = 2,
    CompressionMethodReduced2 = 3,
    CompressionMethodReduced3 = 4,
    CompressionMethodReduced4 = 5,
    CompressionMethodImploded = 6,
    CompressionMethodReservedTokenizing = 7, // reserved for tokenizing
    CompressionMethodDeflated = 8,
    CompressionMethodDeflated64 = 9,
    CompressionMethodPKImploding = 10,

    CompressionMethodBZip2 = 12,

    CompressionMethodLZMA = 14,

    CompressionMethodTerse = 18,
    CompressionMethodLz77 = 19,

    CompressionMethodJpeg = 96,
    CompressionMethodWavPack = 97,
    CompressionMethodPPMd = 98,
    CompressionMethodWzAES = 99
};
Q_DECLARE_TYPEINFO(CompressionMethod, Q_PRIMITIVE_TYPE);

struct LocalFileHeader
{
    uchar signature[4]; //  0x04034b50
    uchar version_needed[2];
    uchar general_purpose_bits[2];
    uchar compression_method[2];
    uchar last_mod_file[4];
    uchar crc_32[4];
    uchar compressed_size[4];
    uchar uncompressed_size[4];
    uchar file_name_length[2];
    uchar extra_field_length[2];
};
Q_DECLARE_TYPEINFO(LocalFileHeader, Q_PRIMITIVE_TYPE);

struct DataDescriptor
{
    uchar crc_32[4];
    uchar compressed_size[4];
    uchar uncompressed_size[4];
};
Q_DECLARE_TYPEINFO(DataDescriptor, Q_PRIMITIVE_TYPE);

struct CentralFileHeader
{
    uchar signature[4]; // 0x02014b50
    uchar version_made[2];
    uchar version_needed[2];
    uchar general_purpose_bits[2];
    uchar compression_method[2];
    uchar last_mod_file[4];
    uchar crc_32[4];
    uchar compressed_size[4];
    uchar uncompressed_size[4];
    uchar file_name_length[2];
    uchar extra_field_length[2];
    uchar file_comment_length[2];
    uchar disk_start[2];
    uchar internal_file_attributes[2];
    uchar external_file_attributes[4];
    uchar offset_local_header[4];
};
Q_DECLARE_TYPEINFO(CentralFileHeader, Q_PRIMITIVE_TYPE);

struct EndOfDirectory
{
    uchar signature[4]; // 0x06054b50
    uchar this_disk[2];
    uchar start_of_directory_disk[2];
    uchar num_dir_entries_this_disk[2];
    uchar num_dir_entries[2];
    uchar directory_size[4];
    uchar dir_start_offset[4];
    uchar comment_length[2];
};
Q_DECLARE_TYPEINFO(EndOfDirectory, Q_PRIMITIVE_TYPE);

struct FileHeader
{
    CentralFileHeader h;
    QByteArray file_name;
    QByteArray extra_field;
    QByteArray file_comment;
};
Q_DECLARE_TYPEINFO(FileHeader, Q_RELOCATABLE_TYPE);

class QZipPrivate
{
public:
    QZipPrivate(QIODevice *device, bool ownDev)
        : device(device), ownDevice(ownDev), dirtyFileTree(true), start_of_directory(0)
    {
    }

    ~QZipPrivate()
    {
        if (ownDevice)
            delete device;
    }

    QZipReader::FileInfo fillFileInfo(int index) const;

    QIODevice *device;
    bool ownDevice;
    bool dirtyFileTree;
    QList<FileHeader> fileHeaders;
    QByteArray comment;
    uint start_of_directory;
};

QZipReader::FileInfo QZipPrivate::fillFileInfo(int index) const
{
    QZipReader::FileInfo fileInfo;
    FileHeader header = fileHeaders.at(index);
    quint32 mode = readUInt(header.h.external_file_attributes);
    const HostOS hostOS = HostOS(readUShort(header.h.version_made) >> 8);
    switch (hostOS) {
    case HostUnix:
        mode = (mode >> 16) & 0xffff;
        switch (mode & UnixFileAttributes::TypeMask) {
        case UnixFileAttributes::SymLink:
            fileInfo.isSymLink = true;
            break;
        case UnixFileAttributes::Dir:
            fileInfo.isDir = true;
            break;
        case UnixFileAttributes::File:
        default: // ### just for the case; should we warn?
            fileInfo.isFile = true;
            break;
        }
        fileInfo.permissions = modeToPermissions(mode);
        break;
    case HostFAT:
    case HostNTFS:
    case HostHPFS:
    case HostVFAT:
        switch (mode & WindowsFileAttributes::TypeMask) {
        case WindowsFileAttributes::Dir:
            fileInfo.isDir = true;
            break;
        case WindowsFileAttributes::File:
        default:
            fileInfo.isFile = true;
            break;
        }
        fileInfo.permissions |= QFile::ReadOwner | QFile::ReadUser | QFile::ReadGroup | QFile::ReadOther;
        if ((mode & WindowsFileAttributes::ReadOnly) == 0)
            fileInfo.permissions |= QFile::WriteOwner | QFile::WriteUser | QFile::WriteGroup | QFile::WriteOther;
        if (fileInfo.isDir)
            fileInfo.permissions |= QFile::ExeOwner | QFile::ExeUser | QFile::ExeGroup | QFile::ExeOther;
        break;
    default:
        qWarning("QZip: Zip entry format at %d is not supported.", index);
        return fileInfo; // we don't support anything else
    }

    ushort general_purpose_bits = readUShort(header.h.general_purpose_bits);
    // if bit 11 is set, the filename and comment fields must be encoded using UTF-8
    const bool inUtf8 = (general_purpose_bits & Utf8Names) != 0;
    fileInfo.filePath = inUtf8 ? QString::fromUtf8(header.file_name) : QString::fromLocal8Bit(header.file_name);
    fileInfo.crc = readUInt(header.h.crc_32);
    fileInfo.size = readUInt(header.h.uncompressed_size);
    fileInfo.lastModified = readMSDosDate(header.h.last_mod_file);

    // fix the file path, if broken (convert separators, eat leading and trailing ones)
    fileInfo.filePath = QDir::fromNativeSeparators(fileInfo.filePath);
    QStringView filePathRef(fileInfo.filePath);
    while (filePathRef.startsWith(u'.') || filePathRef.startsWith(u'/'))
        filePathRef = filePathRef.mid(1);
    while (filePathRef.endsWith(u'/'))
        filePathRef.chop(1);

    fileInfo.filePath = filePathRef.toString();
    return fileInfo;
}

class QZipReaderPrivate : public QZipPrivate
{
public:
    QZipReaderPrivate(QIODevice *device, bool ownDev)
        : QZipPrivate(device, ownDev), status(QZipReader::NoError)
    {
    }

    void scanFiles();

    QZipReader::Status status;
};

class QZipWriterPrivate : public QZipPrivate
{
public:
    QZipWriterPrivate(QIODevice *device, bool ownDev)
        : QZipPrivate(device, ownDev),
        status(QZipWriter::NoError),
        permissions(QFile::ReadOwner | QFile::WriteOwner),
        compressionPolicy(QZipWriter::AlwaysCompress)
    {
    }

    QZipWriter::Status status;
    QFile::Permissions permissions;
    QZipWriter::CompressionPolicy compressionPolicy;

    enum EntryType { Directory, File, Symlink };

    void addEntry(EntryType type, const QString &fileName, const QByteArray &contents);
};

static LocalFileHeader toLocalHeader(const CentralFileHeader &ch)
{
    LocalFileHeader h;
    writeUInt(h.signature, 0x04034b50);
    copyUShort(h.version_needed, ch.version_needed);
    copyUShort(h.general_purpose_bits, ch.general_purpose_bits);
    copyUShort(h.compression_method, ch.compression_method);
    copyUInt(h.last_mod_file, ch.last_mod_file);
    copyUInt(h.crc_32, ch.crc_32);
    copyUInt(h.compressed_size, ch.compressed_size);
    copyUInt(h.uncompressed_size, ch.uncompressed_size);
    copyUShort(h.file_name_length, ch.file_name_length);
    copyUShort(h.extra_field_length, ch.extra_field_length);
    return h;
}

void QZipReaderPrivate::scanFiles()
{
    if (!dirtyFileTree)
        return;

    if (! (device->isOpen() || device->open(QIODevice::ReadOnly))) {
        status = QZipReader::FileOpenError;
        return;
    }

    if ((device->openMode() & QIODevice::ReadOnly) == 0) { // only read the index from readable files.
        status = QZipReader::FileReadError;
        return;
    }

    dirtyFileTree = false;
    uchar tmp[4];
    device->read((char *)tmp, 4);
    if (readUInt(tmp) != 0x04034b50) {
        qWarning("QZip: not a zip file!");
        return;
    }

    // find EndOfDirectory header
    int i = 0;
    int start_of_directory = -1;
    int num_dir_entries = 0;
    EndOfDirectory eod;
    while (start_of_directory == -1) {
        const int pos = device->size() - int(sizeof(EndOfDirectory)) - i;
        if (pos < 0 || i > 65535) {
            qWarning("QZip: EndOfDirectory not found");
            return;
        }

        device->seek(pos);
        device->read((char *)&eod, sizeof(EndOfDirectory));
        if (readUInt(eod.signature) == 0x06054b50)
            break;
        ++i;
    }

    // have the eod
    start_of_directory = readUInt(eod.dir_start_offset);
    num_dir_entries = readUShort(eod.num_dir_entries);
    ZDEBUG("start_of_directory at %d, num_dir_entries=%d", start_of_directory, num_dir_entries);
    int comment_length = readUShort(eod.comment_length);
    if (comment_length != i)
        qWarning("QZip: failed to parse zip file.");
    comment = device->read(qMin(comment_length, i));


    device->seek(start_of_directory);
    for (i = 0; i < num_dir_entries; ++i) {
        FileHeader header;
        int read = device->read((char *) &header.h, sizeof(CentralFileHeader));
        if (read < (int)sizeof(CentralFileHeader)) {
            qWarning("QZip: Failed to read complete header, index may be incomplete");
            break;
        }
        if (readUInt(header.h.signature) != 0x02014b50) {
            qWarning("QZip: invalid header signature, index may be incomplete");
            break;
        }

        int l = readUShort(header.h.file_name_length);
        header.file_name = device->read(l);
        if (header.file_name.size() != l) {
            qWarning("QZip: Failed to read filename from zip index, index may be incomplete");
            break;
        }
        l = readUShort(header.h.extra_field_length);
        header.extra_field = device->read(l);
        if (header.extra_field.size() != l) {
            qWarning("QZip: Failed to read extra field in zip file, skipping file, index may be incomplete");
            break;
        }
        l = readUShort(header.h.file_comment_length);
        header.file_comment = device->read(l);
        if (header.file_comment.size() != l) {
            qWarning("QZip: Failed to read read file comment, index may be incomplete");
            break;
        }

        ZDEBUG("found file '%s'", header.file_name.data());
        fileHeaders.append(header);
    }
}

void QZipWriterPrivate::addEntry(EntryType type, const QString &fileName, const QByteArray &contents/*, QFile::Permissions permissions, QZip::Method m*/)
{
#ifndef NDEBUG
    static const char *const entryTypes[] = {
        "directory",
        "file     ",
        "symlink  " };
    ZDEBUG() << "adding" << entryTypes[type] <<":" << fileName.toUtf8().data() << (type == 2 ? QByteArray(" -> " + contents).constData() : "");
#endif

    if (! (device->isOpen() || device->open(QIODevice::WriteOnly))) {
        status = QZipWriter::FileOpenError;
        return;
    }
    device->seek(start_of_directory);

    // don't compress small files
    QZipWriter::CompressionPolicy compression = compressionPolicy;
    if (compressionPolicy == QZipWriter::AutoCompress) {
        if (contents.size() < 64)
            compression = QZipWriter::NeverCompress;
        else
            compression = QZipWriter::AlwaysCompress;
    }

    FileHeader header;
    memset(&header.h, 0, sizeof(CentralFileHeader));
    writeUInt(header.h.signature, 0x02014b50);

    writeUShort(header.h.version_needed, ZIP_VERSION);
    writeUInt(header.h.uncompressed_size, contents.size());
    writeMSDosDate(header.h.last_mod_file, QDateTime::currentDateTime());
    QByteArray data = contents;
    if (compression == QZipWriter::AlwaysCompress) {
        writeUShort(header.h.compression_method, CompressionMethodDeflated);

       ulong len = contents.size();
        // shamelessly copied form zlib
        len += (len >> 12) + (len >> 14) + 11;
        int res;
        do {
            data.resize(len);
            res = deflate((uchar*)data.data(), &len, (const uchar*)contents.constData(), contents.size());

            switch (res) {
            case Z_OK:
                data.resize(len);
                break;
            case Z_MEM_ERROR:
                qWarning("QZip: Z_MEM_ERROR: Not enough memory to compress file, skipping");
                data.resize(0);
                break;
            case Z_BUF_ERROR:
                len *= 2;
                break;
            }
        } while (res == Z_BUF_ERROR);
    }
// TODO add a check if data.length() > contents.length().  Then try to store the original and revert the compression method to be uncompressed
    writeUInt(header.h.compressed_size, data.size());
    uint crc_32 = ::crc32(0, nullptr, 0);
    crc_32 = ::crc32(crc_32, (const uchar *)contents.constData(), contents.size());
    writeUInt(header.h.crc_32, crc_32);

    // if bit 11 is set, the filename and comment fields must be encoded using UTF-8
    ushort general_purpose_bits = Utf8Names; // always use utf-8
    writeUShort(header.h.general_purpose_bits, general_purpose_bits);

    const bool inUtf8 = (general_purpose_bits & Utf8Names) != 0;
    header.file_name = inUtf8 ? fileName.toUtf8() : fileName.toLocal8Bit();
    if (header.file_name.size() > 0xffff) {
        qWarning("QZip: Filename is too long, chopping it to 65535 bytes");
        header.file_name = header.file_name.left(0xffff); // ### don't break the utf-8 sequence, if any
    }
    if (header.file_comment.size() + header.file_name.size() > 0xffff) {
        qWarning("QZip: File comment is too long, chopping it to 65535 bytes");
        header.file_comment.truncate(0xffff - header.file_name.size()); // ### don't break the utf-8 sequence, if any
    }
    writeUShort(header.h.file_name_length, header.file_name.size());
    //h.extra_field_length[2];

    writeUShort(header.h.version_made, HostUnix << 8);
    //uchar internal_file_attributes[2];
    //uchar external_file_attributes[4];
    quint32 mode = permissionsToMode(permissions);
    switch (type) {
    case Symlink:
        mode |= UnixFileAttributes::SymLink;
        break;
    case Directory:
        mode |= UnixFileAttributes::Dir;
        break;
    case File:
        mode |= UnixFileAttributes::File;
        break;
    default:
        Q_UNREACHABLE();
        break;
    }
    writeUInt(header.h.external_file_attributes, mode << 16);
    writeUInt(header.h.offset_local_header, start_of_directory);


    fileHeaders.append(header);

    LocalFileHeader h = toLocalHeader(header.h);
    device->write((const char *)&h, sizeof(LocalFileHeader));
    device->write(header.file_name);
    device->write(data);
    start_of_directory = device->pos();
    dirtyFileTree = true;
}

//////////////////////////////  Reader

/*!
    \class QZipReader::FileInfo
    \internal
    Represents one entry in the zip table of contents.
*/

/*!
    \variable QZipReader::FileInfo::filePath
    The full filepath inside the archive.
*/

/*!
    \variable QZipReader::FileInfo::isDir
    A boolean type indicating if the entry is a directory.
*/

/*!
    \variable QZipReader::FileInfo::isFile
    A boolean type, if it is one this entry is a file.
*/

/*!
    \variable QZipReader::FileInfo::isSymLink
    A boolean type, if it is one this entry is symbolic link.
*/

/*!
    \variable QZipReader::FileInfo::permissions
    A list of flags for the permissions of this entry.
*/

/*!
    \variable QZipReader::FileInfo::crc
    The calculated checksum as a crc type.
*/

/*!
    \variable QZipReader::FileInfo::size
    The total size of the unpacked content.
*/

/*!
    \class QZipReader
    \internal
    \since 4.5

    \brief the QZipReader class provides a way to inspect the contents of a zip
    archive and extract individual files from it.

    QZipReader can be used to read a zip archive either from a file or from any
    device. An in-memory QBuffer for instance.  The reader can be used to read
    which files are in the archive using fileInfoList() and entryInfoAt() but
    also to extract individual files using fileData() or even to extract all
    files in the archive using extractAll()
*/

/*!
    Create a new zip archive that operates on the \a fileName.  The file will be
    opened with the \a mode.
*/
QZipReader::QZipReader(const QString &archive, QIODevice::OpenMode mode)
{
    auto f = std::make_unique<QFile>(archive);
    const bool result = f->open(mode);
    QZipReader::Status status;
    const QFileDevice::FileError error = f->error();
    if (result && error == QFile::NoError) {
        status = NoError;
    } else {
        if (error == QFile::ReadError)
            status = FileReadError;
        else if (error == QFile::OpenError)
            status = FileOpenError;
        else if (error == QFile::PermissionsError)
            status = FilePermissionsError;
        else
            status = FileError;
    }

    d = new QZipReaderPrivate(f.get(), /*ownDevice=*/true);
    Q_UNUSED(f.release());
    d->status = status;
}

/*!
    Create a new zip archive that operates on the archive found in \a device.
    You have to open the device previous to calling the constructor and only a
    device that is readable will be scanned for zip filecontent.
 */
QZipReader::QZipReader(QIODevice *device)
    : d(new QZipReaderPrivate(device, /*ownDevice=*/false))
{
    Q_ASSERT(device);
}

/*!
    Destructor
*/
QZipReader::~QZipReader()
{
    close();
    delete d;
}

/*!
    Returns device used for reading zip archive.
*/
QIODevice* QZipReader::device() const
{
    return d->device;
}

/*!
    Returns \c true if the user can read the file; otherwise returns \c false.
*/
bool QZipReader::isReadable() const
{
    return d->device->isReadable();
}

/*!
    Returns \c true if the file exists; otherwise returns \c false.
*/
bool QZipReader::exists() const
{
    QFile *f = qobject_cast<QFile*> (d->device);
    if (f == nullptr)
        return true;
    return f->exists();
}

/*!
    Returns the list of files the archive contains.
*/
QList<QZipReader::FileInfo> QZipReader::fileInfoList() const
{
    d->scanFiles();
    QList<FileInfo> files;
    const int numFileHeaders = d->fileHeaders.size();
    files.reserve(numFileHeaders);
    for (int i = 0; i < numFileHeaders; ++i)
        files.append(d->fillFileInfo(i));
    return files;

}

/*!
    Return the number of items in the zip archive.
*/
int QZipReader::count() const
{
    d->scanFiles();
    return d->fileHeaders.size();
}

/*!
    Returns a FileInfo of an entry in the zipfile.
    The \a index is the index into the directory listing of the zipfile.
    Returns an invalid FileInfo if \a index is out of boundaries.

    \sa fileInfoList()
*/
QZipReader::FileInfo QZipReader::entryInfoAt(int index) const
{
    d->scanFiles();
    if (index >= 0 && index < d->fileHeaders.size())
        return d->fillFileInfo(index);
    return QZipReader::FileInfo();
}

/*!
    Fetch the file contents from the zip archive and return the uncompressed bytes.
*/
QByteArray QZipReader::fileData(const QString &fileName) const
{
    d->scanFiles();
    int i;
    for (i = 0; i < d->fileHeaders.size(); ++i) {
        if (QString::fromLocal8Bit(d->fileHeaders.at(i).file_name) == fileName)
            break;
    }
    if (i == d->fileHeaders.size())
        return QByteArray();

    FileHeader header = d->fileHeaders.at(i);

    ushort version_needed = readUShort(header.h.version_needed);
    if (version_needed > ZIP_VERSION) {
        qWarning("QZip: .ZIP specification version %d implementationis needed to extract the data.", version_needed);
        return QByteArray();
    }

    ushort general_purpose_bits = readUShort(header.h.general_purpose_bits);
    int compressed_size = readUInt(header.h.compressed_size);
    int uncompressed_size = readUInt(header.h.uncompressed_size);
    int start = readUInt(header.h.offset_local_header);
    //qDebug("uncompressing file %d: local header at %d", i, start);

    d->device->seek(start);
    LocalFileHeader lh;
    d->device->read((char *)&lh, sizeof(LocalFileHeader));
    uint skip = readUShort(lh.file_name_length) + readUShort(lh.extra_field_length);
    d->device->seek(d->device->pos() + skip);

    int compression_method = readUShort(lh.compression_method);
    //qDebug("file=%s: compressed_size=%d, uncompressed_size=%d", fileName.toLocal8Bit().data(), compressed_size, uncompressed_size);

    if ((general_purpose_bits & Encrypted) != 0) {
        qWarning("QZip: Unsupported encryption method is needed to extract the data.");
        return QByteArray();
    }

    //qDebug("file at %lld", d->device->pos());
    QByteArray compressed = d->device->read(compressed_size);
    if (compression_method == CompressionMethodStored) {
        // no compression
        compressed.truncate(uncompressed_size);
        return compressed;
    } else if (compression_method == CompressionMethodDeflated) {
        // Deflate
        //qDebug("compressed=%d", compressed.size());
        compressed.truncate(compressed_size);
        QByteArray baunzip;
        ulong len = qMax(uncompressed_size,  1);
        int res;
        do {
            baunzip.resize(len);
            res = inflate((uchar*)baunzip.data(), &len,
                          (const uchar*)compressed.constData(), compressed_size);

            switch (res) {
            case Z_OK:
                if ((int)len != baunzip.size())
                    baunzip.resize(len);
                break;
            case Z_MEM_ERROR:
                qWarning("QZip: Z_MEM_ERROR: Not enough memory");
                break;
            case Z_BUF_ERROR:
                len *= 2;
                break;
            case Z_DATA_ERROR:
                qWarning("QZip: Z_DATA_ERROR: Input data is corrupted");
                break;
            }
        } while (res == Z_BUF_ERROR);
        return baunzip;
    }

    qWarning("QZip: Unsupported compression method %d is needed to extract the data.", compression_method);
    return QByteArray();
}

/*!
    Extracts the full contents of the zip file into \a destinationDir on
    the local filesystem.
    In case writing or linking a file fails, the extraction will be aborted.
*/
bool QZipReader::extractAll(const QString &destinationDir) const
{
    QDir baseDir(destinationDir);

    // create directories first
    const QList<FileInfo> allFiles = fileInfoList();
    bool foundDirs = false;
    bool hasDirs = false;
    for (const FileInfo &fi : allFiles) {
        const QString absPath = destinationDir + QDir::separator() + fi.filePath;
        if (fi.isDir) {
            foundDirs = true;
            if (!baseDir.mkpath(fi.filePath))
                return false;
            if (!QFile::setPermissions(absPath, fi.permissions))
                return false;
        } else if (!hasDirs && fi.filePath.contains(u"/")) {
            // filePath does not have leading or trailing '/', so if we find
            // one, than the file path contains directories.
            hasDirs = true;
        }
    }

    // Some zip archives can be broken in the sense that they do not report
    // separate entries for directories, only for files. In this case we
    // need to recreate directory structure based on the file paths.
    if (hasDirs && !foundDirs) {
        for (const FileInfo &fi : allFiles) {
            const auto dirPath = fi.filePath.left(fi.filePath.lastIndexOf(u"/"));
            if (!baseDir.mkpath(dirPath))
                return false;
            // We will leave the directory permissions default in this case,
            // because setting dir permissions based on file is incorrect
        }
    }

    // set up symlinks
    for (const FileInfo &fi : allFiles) {
        const QString absPath = destinationDir + QDir::separator() + fi.filePath;
        if (fi.isSymLink) {
            QString destination = QFile::decodeName(fileData(fi.filePath));
            if (destination.isEmpty())
                return false;
            QFileInfo linkFi(absPath);
            if (!QFile::exists(linkFi.absolutePath()))
                QDir::root().mkpath(linkFi.absolutePath());
            if (!QFile::link(destination, absPath))
                return false;
            /* cannot change permission of links
            if (!QFile::setPermissions(absPath, fi.permissions))
                return false;
            */
        }
    }

    for (const FileInfo &fi : allFiles) {
        const QString absPath = destinationDir + QDir::separator() + fi.filePath;
        if (fi.isFile) {
            QFile f(absPath);
            if (!f.open(QIODevice::WriteOnly))
                return false;
            f.write(fileData(fi.filePath));
            f.setPermissions(fi.permissions);
            f.close();
        }
    }

    return true;
}

/*!
    \enum QZipReader::Status

    The following status values are possible:

    \value NoError  No error occurred.
    \value FileReadError    An error occurred when reading from the file.
    \value FileOpenError    The file could not be opened.
    \value FilePermissionsError The file could not be accessed.
    \value FileError        Another file error occurred.
*/

/*!
    Returns a status code indicating the first error that was met by QZipReader,
    or QZipReader::NoError if no error occurred.
*/
QZipReader::Status QZipReader::status() const
{
    return d->status;
}

/*!
    Close the zip file.
*/
void QZipReader::close()
{
    d->device->close();
}

////////////////////////////// Writer

/*!
    \class QZipWriter
    \internal
    \since 4.5

    \brief the QZipWriter class provides a way to create a new zip archive.

    QZipWriter can be used to create a zip archive containing any number of files
    and directories. The files in the archive will be compressed in a way that is
    compatible with common zip reader applications.
*/


/*!
    Create a new zip archive that operates on the \a archive filename.  The file will
    be opened with the \a mode.
    \sa isValid()
*/
QZipWriter::QZipWriter(const QString &fileName, QIODevice::OpenMode mode)
{
    auto f = std::make_unique<QFile>(fileName);
    QZipWriter::Status status;
    if (f->open(mode) && f->error() == QFile::NoError)
        status = QZipWriter::NoError;
    else {
        if (f->error() == QFile::WriteError)
            status = QZipWriter::FileWriteError;
        else if (f->error() == QFile::OpenError)
            status = QZipWriter::FileOpenError;
        else if (f->error() == QFile::PermissionsError)
            status = QZipWriter::FilePermissionsError;
        else
            status = QZipWriter::FileError;
    }

    d = new QZipWriterPrivate(f.get(), /*ownDevice=*/true);
    Q_UNUSED(f.release());
    d->status = status;
}

/*!
    Create a new zip archive that operates on the archive found in \a device.
    You have to open the device previous to calling the constructor and
    only a device that is readable will be scanned for zip filecontent.
 */
QZipWriter::QZipWriter(QIODevice *device)
    : d(new QZipWriterPrivate(device, /*ownDevice=*/false))
{
    Q_ASSERT(device);
}

QZipWriter::~QZipWriter()
{
    close();
    delete d;
}

/*!
    Returns device used for writing zip archive.
*/
QIODevice* QZipWriter::device() const
{
    return d->device;
}

/*!
    Returns \c true if the user can write to the archive; otherwise returns \c false.
*/
bool QZipWriter::isWritable() const
{
    return d->device->isWritable();
}

/*!
    Returns \c true if the file exists; otherwise returns \c false.
*/
bool QZipWriter::exists() const
{
    QFile *f = qobject_cast<QFile*> (d->device);
    if (f == nullptr)
        return true;
    return f->exists();
}

/*!
    \enum QZipWriter::Status

    The following status values are possible:

    \value NoError  No error occurred.
    \value FileWriteError    An error occurred when writing to the device.
    \value FileOpenError    The file could not be opened.
    \value FilePermissionsError The file could not be accessed.
    \value FileError        Another file error occurred.
*/

/*!
    Returns a status code indicating the first error that was met by QZipWriter,
    or QZipWriter::NoError if no error occurred.
*/
QZipWriter::Status QZipWriter::status() const
{
    return d->status;
}

/*!
    \enum QZipWriter::CompressionPolicy

    \value AlwaysCompress   A file that is added is compressed.
    \value NeverCompress    A file that is added will be stored without changes.
    \value AutoCompress     A file that is added will be compressed only if that will give a smaller file.
*/

/*!
     Sets the policy for compressing newly added files to the new \a policy.

    \note the default policy is AlwaysCompress

    \sa compressionPolicy()
    \sa addFile()
*/
void QZipWriter::setCompressionPolicy(CompressionPolicy policy)
{
    d->compressionPolicy = policy;
}

/*!
     Returns the currently set compression policy.
    \sa setCompressionPolicy()
    \sa addFile()
*/
QZipWriter::CompressionPolicy QZipWriter::compressionPolicy() const
{
    return d->compressionPolicy;
}

/*!
    Sets the permissions that will be used for newly added files.

    \note the default permissions are QFile::ReadOwner | QFile::WriteOwner.

    \sa creationPermissions()
    \sa addFile()
*/
void QZipWriter::setCreationPermissions(QFile::Permissions permissions)
{
    d->permissions = permissions;
}

/*!
     Returns the currently set creation permissions.

    \sa setCreationPermissions()
    \sa addFile()
*/
QFile::Permissions QZipWriter::creationPermissions() const
{
    return d->permissions;
}

/*!
    Add a file to the archive with \a data as the file contents.
    The file will be stored in the archive using the \a fileName which
    includes the full path in the archive.

    The new file will get the file permissions based on the current
    creationPermissions and it will be compressed using the zip compression
    based on the current compression policy.

    \sa setCreationPermissions()
    \sa setCompressionPolicy()
*/
void QZipWriter::addFile(const QString &fileName, const QByteArray &data)
{
    d->addEntry(QZipWriterPrivate::File, QDir::fromNativeSeparators(fileName), data);
}

/*!
    Add a file to the archive with \a device as the source of the contents.
    The contents returned from QIODevice::readAll() will be used as the
    filedata.
    The file will be stored in the archive using the \a fileName which
    includes the full path in the archive.
*/
void QZipWriter::addFile(const QString &fileName, QIODevice *device)
{
    Q_ASSERT(device);
    QIODevice::OpenMode mode = device->openMode();
    bool opened = false;
    if ((mode & QIODevice::ReadOnly) == 0) {
        opened = true;
        if (! device->open(QIODevice::ReadOnly)) {
            d->status = FileOpenError;
            return;
        }
    }
    d->addEntry(QZipWriterPrivate::File, QDir::fromNativeSeparators(fileName), device->readAll());
    if (opened)
        device->close();
}

/*!
    Create a new directory in the archive with the specified \a dirName and
    the \a permissions;
*/
void QZipWriter::addDirectory(const QString &dirName)
{
    QString name(QDir::fromNativeSeparators(dirName));
    // separator is mandatory
    if (!name.endsWith(u'/'))
        name.append(u'/');
    d->addEntry(QZipWriterPrivate::Directory, name, QByteArray());
}

/*!
    Create a new symbolic link in the archive with the specified \a dirName
    and the \a permissions;
    A symbolic link contains the destination (relative) path and name.
*/
void QZipWriter::addSymLink(const QString &fileName, const QString &destination)
{
    d->addEntry(QZipWriterPrivate::Symlink, QDir::fromNativeSeparators(fileName), QFile::encodeName(destination));
}

/*!
   Closes the zip file.
*/
void QZipWriter::close()
{
    if (!(d->device->openMode() & QIODevice::WriteOnly)) {
        d->device->close();
        return;
    }

    //qDebug("QZip::close writing directory, %d entries", d->fileHeaders.size());
    d->device->seek(d->start_of_directory);
    // write new directory
    for (int i = 0; i < d->fileHeaders.size(); ++i) {
        const FileHeader &header = d->fileHeaders.at(i);
        d->device->write((const char *)&header.h, sizeof(CentralFileHeader));
        d->device->write(header.file_name);
        d->device->write(header.extra_field);
        d->device->write(header.file_comment);
    }
    int dir_size = d->device->pos() - d->start_of_directory;
    // write end of directory
    EndOfDirectory eod;
    memset(&eod, 0, sizeof(EndOfDirectory));
    writeUInt(eod.signature, 0x06054b50);
    //uchar this_disk[2];
    //uchar start_of_directory_disk[2];
    writeUShort(eod.num_dir_entries_this_disk, d->fileHeaders.size());
    writeUShort(eod.num_dir_entries, d->fileHeaders.size());
    writeUInt(eod.directory_size, dir_size);
    writeUInt(eod.dir_start_offset, d->start_of_directory);
    writeUShort(eod.comment_length, d->comment.size());

    d->device->write((const char *)&eod, sizeof(EndOfDirectory));
    d->device->write(d->comment);
    d->device->close();
}

QT_END_NAMESPACE
