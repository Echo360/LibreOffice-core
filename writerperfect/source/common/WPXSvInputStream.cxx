/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <writerperfect/WPXSvInputStream.hxx>

#include <com/sun/star/packages/zip/XZipFileAccess2.hpp>
#include <com/sun/star/uno/Any.hxx>

#include <comphelper/processfactory.hxx>
#include <comphelper/seekableinput.hxx>

#include <rtl/string.hxx>

#include <sot/storage.hxx>

#include <tools/stream.hxx>
#include <unotools/streamwrap.hxx>
#include <unotools/ucbstreamhelper.hxx>

#include <limits>
#include <vector>

#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/unordered_map.hpp>

namespace writerperfect
{

using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::io;

namespace container = com::sun::star::container;
namespace lang = com::sun::star::lang;
namespace packages = com::sun::star::packages;

namespace
{

class PositionHolder : boost::noncopyable
{
public:
    explicit PositionHolder(const Reference<XSeekable> &rxSeekable);
    ~PositionHolder();

private:
    const Reference<XSeekable> mxSeekable;
    const sal_uInt64 mnPosition;
};

PositionHolder::PositionHolder(const Reference<XSeekable> &rxSeekable)
    : mxSeekable(rxSeekable)
    , mnPosition(rxSeekable->getPosition())
{
}

PositionHolder::~PositionHolder() try
{
    mxSeekable->seek(mnPosition);
}
catch (...)
{
}

} // anonymous namespace

typedef struct
{
    SotStorageRef ref;
} SotStorageRefWrapper;

typedef struct
{
    SotStorageStreamRef ref;
} SotStorageStreamRefWrapper;

namespace
{

rtl::OUString lcl_normalizeSubStreamPath(const rtl::OUString &rPath)
{
    // accept paths which begin by '/'
    // TODO: maybe this should to a full normalization
    if (rPath.startsWith("/") && rPath.getLength() >= 2)
        return rPath.copy(1);
    return rPath;
}

}

namespace
{

const rtl::OUString concatPath(const rtl::OUString &lhs, const rtl::OUString &rhs)
{
    if (lhs.isEmpty())
        return rhs;
    return lhs + "/" + rhs;
}

struct OLEStreamData
{
    explicit OLEStreamData(const rtl::OString &rName);

    SotStorageStreamRefWrapper stream;

    /** Name of the stream.
      *
      * This is not @c rtl::OUString, because we need to be able to
      * produce const char* from it.
      */
    rtl::OString name;
};

typedef boost::unordered_map<rtl::OUString, std::size_t, rtl::OUStringHash> NameMap_t;
typedef boost::unordered_map<rtl::OUString, SotStorageRefWrapper, rtl::OUStringHash> OLEStorageMap_t;

/** Representation of an OLE2 storage.
  *
  * This class tries to bring a bit of sanity to use of SotStorage with
  * respect to the needs of @c librevenge::RVNGInputStream API. It
  * holds all nested storages for the whole lifetime (more precisely,
  * since initialization, which is performed by calling @c
  * initialize()), thus ensuring that no created stream is destroyed
  * just because its parent storage went out of scope. It also holds a
  * bidirectional map of stream names to their indexes (index of a
  * stream is determined by deep-first traversal), which is also
  * populated during initialization (member variables @c maStreams and
  * @c maNameMap).
  *
  * Streams are created on demand (and saved, for the same reason as
  * storages).
  */
struct OLEStorageImpl
{
    OLEStorageImpl();

    void initialize(SvStream *pStream);

    SotStorageStreamRef getStream(const rtl::OUString &rPath);
    SotStorageStreamRef getStream(std::size_t nId);

private:
    void traverse(const SotStorageRef &rStorage, const rtl::OUString &rPath);

    SotStorageStreamRef createStream(const rtl::OUString &rPath);

public:
    SotStorageRefWrapper mxRootStorage; //< root storage of the OLE2
    OLEStorageMap_t maStorageMap; //< map of all sub storages by name
    ::std::vector< OLEStreamData > maStreams; //< list of streams and their names
    NameMap_t maNameMap; //< map of stream names to indexes (into @c maStreams)
    bool mbInitialized;
};

OLEStreamData::OLEStreamData(const rtl::OString &rName)
    : stream()
    , name(rName)
{
}

OLEStorageImpl::OLEStorageImpl()
    : mxRootStorage()
    , maStorageMap()
    , maStreams()
    , maNameMap()
    , mbInitialized(false)
{
}

void OLEStorageImpl::initialize(SvStream *const pStream)
{
    if (!pStream)
        return;

    mxRootStorage.ref = new SotStorage( pStream, true );

    traverse(mxRootStorage.ref, "");

    mbInitialized = true;
}

SotStorageStreamRef OLEStorageImpl::getStream(const rtl::OUString &rPath)
{
    const rtl::OUString aPath(lcl_normalizeSubStreamPath(rPath));
    NameMap_t::iterator aIt = maNameMap.find(aPath);

    // For the while don't return stream in this situation.
    // Later, given how libcdr's zip stream implementation behaves,
    // return the first stream in the storage if there is one.
    if (maNameMap.end() == aIt)
        return SotStorageStreamRef();

    if (!maStreams[aIt->second].stream.ref.Is())
        maStreams[aIt->second].stream.ref = createStream(aPath);

    return maStreams[aIt->second].stream.ref;
}

SotStorageStreamRef OLEStorageImpl::getStream(const std::size_t nId)
{
    if (!maStreams[nId].stream.ref.Is())
        maStreams[nId].stream.ref = createStream(rtl::OStringToOUString(maStreams[nId].name, RTL_TEXTENCODING_UTF8));

    return maStreams[nId].stream.ref;
}

void OLEStorageImpl::traverse(const SotStorageRef &rStorage, const rtl::OUString &rPath)
{
    SvStorageInfoList infos;

    rStorage->FillInfoList(&infos);

    for (SvStorageInfoList::const_iterator aIt = infos.begin(); infos.end() != aIt; ++aIt)
    {
        if (aIt->IsStream())
        {
            maStreams.push_back(OLEStreamData(rtl::OUStringToOString(concatPath(rPath, aIt->GetName()), RTL_TEXTENCODING_UTF8)));
            maNameMap[concatPath(rPath, aIt->GetName())] = maStreams.size() - 1;
        }
        else if (aIt->IsStorage())
        {
            const rtl::OUString aPath = concatPath(rPath, aIt->GetName());
            SotStorageRefWrapper xStorage;
            xStorage.ref = rStorage->OpenSotStorage(aIt->GetName(), STREAM_STD_READ);
            maStorageMap[aPath] = xStorage;

            // deep-first traversal
            traverse(xStorage.ref, aPath);
        }
        else
        {
            assert(false);
        }
    }
}

SotStorageStreamRef OLEStorageImpl::createStream(const rtl::OUString &rPath)
{
    const sal_Int32 nDelim = rPath.lastIndexOf(sal_Unicode('/'));

    if (-1 == nDelim)
        return mxRootStorage.ref->OpenSotStream(rPath, STREAM_STD_READ);

    const rtl::OUString aDir = rPath.copy(0, nDelim);
    const rtl::OUString aName = rPath.copy(nDelim + 1);

    const OLEStorageMap_t::const_iterator aIt = maStorageMap.find(aDir);

    // We can only get there for paths that are present in the OLE.
    // Which means the storage must exist.
    assert(maStorageMap.end() != aIt);

    return aIt->second.ref->OpenSotStream(aName, STREAM_STD_READ);
}

}

namespace
{

struct ZipStreamData
{
    explicit ZipStreamData(const rtl::OString &rName);

    Reference<XInputStream> xStream;

    /** Name of the stream.
      *
      * This is not @c rtl::OUString, because we need to be able to
      * produce const char* from it.
      */
    rtl::OString aName;
};

/** Representation of a Zip storage.
  *
  * This is quite similar to OLEStorageImpl, except that we do not need
  * to keep all storages (folders) open.
  */
struct ZipStorageImpl
{
    ZipStorageImpl(const Reference<container::XNameAccess> &rxContainer);

    /** Initialize for access.
      *
      * This creates a bidirectional map of stream names to their
      * indexes (index of a stream is determined by deep-first
      * traversal).
      */
    void initialize();

    Reference<XInputStream> getStream(const rtl::OUString &rPath);
    Reference<XInputStream> getStream(std::size_t nId);

private:
    void traverse(const Reference<container::XNameAccess> &rxEnum);

    Reference<XInputStream> createStream(const rtl::OUString &rPath);

public:
    Reference<container::XNameAccess> mxContainer; //< root of the Zip
    ::std::vector< ZipStreamData > maStreams; //< list of streams and their names
    NameMap_t maNameMap; //< map of stream names to indexes (into @c maStreams)
    bool mbInitialized;
};

ZipStreamData::ZipStreamData(const rtl::OString &rName)
    : xStream()
    , aName(rName)
{
}

ZipStorageImpl::ZipStorageImpl(const Reference<container::XNameAccess> &rxContainer)
    : mxContainer(rxContainer)
    , maStreams()
    , maNameMap()
    , mbInitialized(false)
{
    assert(mxContainer.is());
}

void ZipStorageImpl::initialize()
{
    traverse(mxContainer);

    mbInitialized = true;
}

Reference<XInputStream> ZipStorageImpl::getStream(const rtl::OUString &rPath)
{
    const rtl::OUString aPath(lcl_normalizeSubStreamPath(rPath));
    NameMap_t::iterator aIt = maNameMap.find(aPath);

    // For the while don't return stream in this situation.
    // Later, given how libcdr's zip stream implementation behaves,
    // return the first stream in the storage if there is one.
    if (maNameMap.end() == aIt)
        return Reference<XInputStream>();

    if (!maStreams[aIt->second].xStream.is())
        maStreams[aIt->second].xStream = createStream(aPath);

    return maStreams[aIt->second].xStream;
}

Reference<XInputStream> ZipStorageImpl::getStream(const std::size_t nId)
{
    if (!maStreams[nId].xStream.is())
        maStreams[nId].xStream = createStream(rtl::OStringToOUString(maStreams[nId].aName, RTL_TEXTENCODING_UTF8));

    return maStreams[nId].xStream;
}

void ZipStorageImpl::traverse(const Reference<container::XNameAccess> &rxContainer)
{
    const Sequence<rtl::OUString> lNames = rxContainer->getElementNames();

    maStreams.reserve(lNames.getLength());

    for (sal_Int32 n = 0; n < lNames.getLength(); ++n)
    {
        if (!lNames[n].endsWithAsciiL("/", 1)) // skip dirs
        {
            maStreams.push_back(ZipStreamData(rtl::OUStringToOString(lNames[n], RTL_TEXTENCODING_UTF8)));
            maNameMap[lNames[n]] = maStreams.size() - 1;
        }
    }
}

Reference<XInputStream> ZipStorageImpl::createStream(const rtl::OUString &rPath)
{
    Reference<XInputStream> xStream;

    try
    {
        const Reference<XInputStream> xInputStream(mxContainer->getByName(rPath), UNO_QUERY_THROW);
        const Reference<XSeekable> xSeekable(xInputStream, UNO_QUERY);

        if (xSeekable.is())
            xStream = xInputStream;
        else
            xStream.set(new comphelper::OSeekableInputWrapper(xInputStream, comphelper::getProcessComponentContext()));
    }
    catch (const Exception &)
    {
        // nothing needed
    }

    return xStream;
}

}

class WPXSvInputStreamImpl
{
public :
    WPXSvInputStreamImpl( ::com::sun::star::uno::Reference<
                      ::com::sun::star::io::XInputStream > xStream );
    ~WPXSvInputStreamImpl();

    bool isStructured();
    unsigned subStreamCount();
    const char *subStreamName(unsigned id);
    bool existsSubStream(const char *name);
    librevenge::RVNGInputStream *getSubStreamByName(const char *name);
    librevenge::RVNGInputStream *getSubStreamById(unsigned id);

    const unsigned char *read(unsigned long numBytes, unsigned long &numBytesRead);
    int seek(long offset);
    long tell();
    bool isEnd();

    void invalidateReadBuffer();

private:
    bool isOLE();
    void ensureOLEIsInitialized();

    bool isZip();
    void ensureZipIsInitialized();

    librevenge::RVNGInputStream *createWPXStream(const SotStorageStreamRef &rxStorage);
    librevenge::RVNGInputStream *createWPXStream(const Reference<XInputStream> &rxStream);

private:
    ::com::sun::star::uno::Reference< ::com::sun::star::io::XInputStream > mxStream;
    ::com::sun::star::uno::Reference< ::com::sun::star::io::XSeekable > mxSeekable;
    ::com::sun::star::uno::Sequence< sal_Int8 > maData;
    boost::scoped_ptr< OLEStorageImpl > mpOLEStorage;
    boost::scoped_ptr< ZipStorageImpl > mpZipStorage;
    bool mbCheckedOLE;
    bool mbCheckedZip;
public:
    sal_Int64 mnLength;
    const unsigned char *mpReadBuffer;
    unsigned long mnReadBufferLength;
    unsigned long mnReadBufferPos;
};

WPXSvInputStreamImpl::WPXSvInputStreamImpl( Reference< XInputStream > xStream ) :
    mxStream(xStream),
    mxSeekable(xStream, UNO_QUERY),
    maData(0),
    mpOLEStorage(0),
    mpZipStorage(0),
    mbCheckedOLE(false),
    mbCheckedZip(false),
    mnLength(0),
    mpReadBuffer(0),
    mnReadBufferLength(0),
    mnReadBufferPos(0)
{
    if (!xStream.is() || !mxStream.is())
        mnLength = 0;
    else
    {
        if (!mxSeekable.is())
            mnLength = 0;
        else
        {
            try
            {
                mnLength = mxSeekable->getLength();
                if (0 < mxSeekable->getPosition())
                    mxSeekable->seek(0);
            }
            catch ( ... )
            {
                SAL_WARN("writerperfect", "mnLength = mxSeekable->getLength() threw exception");
                mnLength = 0;
            }
        }
    }
}

WPXSvInputStreamImpl::~WPXSvInputStreamImpl()
{
}

const unsigned char *WPXSvInputStreamImpl::read(unsigned long numBytes, unsigned long &numBytesRead)
{
    numBytesRead = 0;

    if (numBytes == 0 || isEnd())
        return 0;

    numBytesRead = mxStream->readSomeBytes (maData, numBytes);
    if (numBytesRead == 0)
        return 0;

    return (const unsigned char *)maData.getConstArray();
}

long WPXSvInputStreamImpl::tell()
{
    if ((mnLength == 0) || !mxStream.is() || !mxSeekable.is())
        return -1L;
    else
    {
        sal_Int64 tmpPosition = mxSeekable->getPosition();
        if ((tmpPosition < 0) || (tmpPosition > (std::numeric_limits<long>::max)()))
            return -1L;
        return (long)tmpPosition;
    }
}

int WPXSvInputStreamImpl::seek(long offset)
{
    if ((mnLength == 0) || !mxStream.is() || !mxSeekable.is())
        return -1;

    sal_Int64 tmpPosition = mxSeekable->getPosition();
    if ((tmpPosition < 0) || (tmpPosition > (std::numeric_limits<long>::max)()))
        return -1;

    try
    {
        mxSeekable->seek(offset);
        return 0;
    }
    catch (...)
    {
        SAL_WARN("writerperfect", "mxSeekable->seek(offset) threw exception");
        return -1;
    }
}

bool WPXSvInputStreamImpl::isEnd()
{
    if ((mnLength == 0) || !mxStream.is() || !mxSeekable.is())
        return true;
    return (mxSeekable->getPosition() >= mnLength);
}

bool WPXSvInputStreamImpl::isStructured()
{
    if ((mnLength == 0) || !mxStream.is() || !mxSeekable.is())
        return false;

    PositionHolder pos(mxSeekable);
    mxSeekable->seek(0);

    if (isOLE())
        return true;

    mxSeekable->seek(0);

    return isZip();
}

unsigned WPXSvInputStreamImpl::subStreamCount()
{
    if ((mnLength == 0) || !mxStream.is() || !mxSeekable.is())
        return 0;

    PositionHolder pos(mxSeekable);
    mxSeekable->seek(0);

    if (isOLE())
    {
        ensureOLEIsInitialized();

        return mpOLEStorage->maStreams.size();
    }

    mxSeekable->seek(0);

    if (isZip())
    {
        ensureZipIsInitialized();

        return mpZipStorage->maStreams.size();
    }

    return 0;
}

const char *WPXSvInputStreamImpl::subStreamName(const unsigned id)
{
    if ((mnLength == 0) || !mxStream.is() || !mxSeekable.is())
        return 0;

    PositionHolder pos(mxSeekable);
    mxSeekable->seek(0);

    if (isOLE())
    {
        ensureOLEIsInitialized();

        if (mpOLEStorage->maStreams.size() <= id)
            return 0;

        return mpOLEStorage->maStreams[id].name.getStr();
    }

    mxSeekable->seek(0);

    if (isZip())
    {
        ensureZipIsInitialized();

        if (mpZipStorage->maStreams.size() <= id)
            return 0;

        return mpZipStorage->maStreams[id].aName.getStr();
    }

    return 0;
}

bool WPXSvInputStreamImpl::existsSubStream(const char *const name)
{
    if (!name)
        return false;

    if ((mnLength == 0) || !mxStream.is() || !mxSeekable.is())
        return false;

    PositionHolder pos(mxSeekable);
    mxSeekable->seek(0);

    const rtl::OUString aName(rtl::OStringToOUString(rtl::OString(name), RTL_TEXTENCODING_UTF8));

    if (isOLE())
    {
        ensureOLEIsInitialized();
        return mpOLEStorage->maNameMap.end() != mpOLEStorage->maNameMap.find(aName);
    }

    mxSeekable->seek(0);

    if (isZip())
    {
        ensureZipIsInitialized();
        return mpZipStorage->maNameMap.end() != mpZipStorage->maNameMap.find(aName);
    }

    return false;
}

librevenge::RVNGInputStream *WPXSvInputStreamImpl::getSubStreamByName(const char *const name)
{
    if (!name)
        return 0;

    if ((mnLength == 0) || !mxStream.is() || !mxSeekable.is())
        return 0;

    PositionHolder pos(mxSeekable);
    mxSeekable->seek(0);

    const rtl::OUString aName(rtl::OStringToOUString(rtl::OString(name), RTL_TEXTENCODING_UTF8));

    if (isOLE())
    {
        ensureOLEIsInitialized();
        return createWPXStream(mpOLEStorage->getStream(aName));
    }

    mxSeekable->seek(0);

    if (isZip())
    {
        ensureZipIsInitialized();

        try
        {
            return createWPXStream(mpZipStorage->getStream(aName));
        }
        catch (const Exception &)
        {
            // nothing needed
        }
    }

    return 0;
}

librevenge::RVNGInputStream *WPXSvInputStreamImpl::getSubStreamById(const unsigned id)
{
    if ((mnLength == 0) || !mxStream.is() || !mxSeekable.is())
        return 0;

    PositionHolder pos(mxSeekable);
    mxSeekable->seek(0);

    if (isOLE())
    {
        ensureOLEIsInitialized();

        if (mpOLEStorage->maStreams.size() <= id)
            return 0;

        return createWPXStream(mpOLEStorage->getStream(id));
    }

    mxSeekable->seek(0);

    if (isZip())
    {
        ensureZipIsInitialized();

        if (mpZipStorage->maStreams.size() <= id)
            return 0;

        try
        {
            return createWPXStream(mpZipStorage->getStream(id));
        }
        catch (const Exception &)
        {
            // nothing needed
        }
    }
    return 0;
}

void WPXSvInputStreamImpl::invalidateReadBuffer()
{
    if (mpReadBuffer)
    {
        seek((long) tell() + (long)mnReadBufferPos - (long)mnReadBufferLength);
        mpReadBuffer = 0;
        mnReadBufferPos = 0;
        mnReadBufferLength = 0;
    }
}

librevenge::RVNGInputStream *WPXSvInputStreamImpl::createWPXStream(const SotStorageStreamRef &rxStorage)
{
    if (rxStorage.Is())
    {
        Reference < XInputStream > xContents(new utl::OSeekableInputStreamWrapper( rxStorage ));
        return new WPXSvInputStream( xContents );
    }
    return 0;
}

librevenge::RVNGInputStream *WPXSvInputStreamImpl::createWPXStream(const Reference<XInputStream> &rxStream)
{
    if (rxStream.is())
        return new WPXSvInputStream( rxStream );
    else
        return 0;
}

bool WPXSvInputStreamImpl::isOLE()
{
    if (!mbCheckedOLE)
    {
        assert(0 == mxSeekable->getPosition());

        boost::scoped_ptr<SvStream> pStream(utl::UcbStreamHelper::CreateStream( mxStream ));
        if (pStream && SotStorage::IsOLEStorage(pStream.get()))
            mpOLEStorage.reset(new OLEStorageImpl());

        mbCheckedOLE = true;
    }

    return bool(mpOLEStorage);
}

bool WPXSvInputStreamImpl::isZip()
{
    if (!mbCheckedZip)
    {
        assert(0 == mxSeekable->getPosition());

        try
        {
            Sequence<Any> aArgs(1);
            aArgs[0] <<= mxStream;

            const Reference<XComponentContext> xContext(comphelper::getProcessComponentContext(), UNO_QUERY_THROW);
            const Reference<packages::zip::XZipFileAccess2> xZip(
                    xContext->getServiceManager()->createInstanceWithArgumentsAndContext("com.sun.star.packages.zip.ZipFileAccess", aArgs, xContext),
                    UNO_QUERY_THROW);
            mpZipStorage.reset(new ZipStorageImpl(xZip));
        }
        catch (const Exception &)
        {
            // ignore
        }

        mbCheckedZip = true;
    }

    return bool(mpZipStorage);
}

void WPXSvInputStreamImpl::ensureOLEIsInitialized()
{
    assert(mpOLEStorage);

    if (!mpOLEStorage->mbInitialized)
        mpOLEStorage->initialize(utl::UcbStreamHelper::CreateStream( mxStream ));
}

void WPXSvInputStreamImpl::ensureZipIsInitialized()
{
    assert(mpZipStorage);

    if (!mpZipStorage->mbInitialized)
        mpZipStorage->initialize();
}

WPXSvInputStream::WPXSvInputStream( Reference< XInputStream > xStream ) :
    mpImpl(new WPXSvInputStreamImpl(xStream))
{
}

WPXSvInputStream::~WPXSvInputStream()
{
   if (mpImpl)
       delete mpImpl;
}

#define BUFFER_MAX 65536

const unsigned char *WPXSvInputStream::read(unsigned long numBytes, unsigned long &numBytesRead)
{
    numBytesRead = 0;

    if (numBytes == 0 || numBytes > (std::numeric_limits<unsigned long>::max)()/2)
        return 0;

    if (mpImpl->mpReadBuffer)
    {
        if ((mpImpl->mnReadBufferPos + numBytes > mpImpl->mnReadBufferPos) && (mpImpl->mnReadBufferPos + numBytes <= mpImpl->mnReadBufferLength))
        {
            const unsigned char *pTmp = mpImpl->mpReadBuffer + mpImpl->mnReadBufferPos;
            mpImpl->mnReadBufferPos += numBytes;
            numBytesRead = numBytes;
            return pTmp;
        }

        mpImpl->invalidateReadBuffer();
    }

    unsigned long curpos = (unsigned long) mpImpl->tell();
    if (curpos == (unsigned long)-1)  // returned ERROR
        return 0;

    if ((curpos + numBytes < curpos) /*overflow*/ ||
            (curpos + numBytes >= (sal_uInt64)mpImpl->mnLength))  /*reading more than available*/
    {
        numBytes = mpImpl->mnLength - curpos;
    }

    if (numBytes < BUFFER_MAX)
    {
        if (BUFFER_MAX < mpImpl->mnLength - curpos)
            mpImpl->mnReadBufferLength = BUFFER_MAX;
        else /* BUFFER_MAX >= mpImpl->mnLength - curpos */
            mpImpl->mnReadBufferLength = mpImpl->mnLength - curpos;
    }
    else
        mpImpl->mnReadBufferLength = numBytes;

    unsigned long tmpNumBytes(0);
    mpImpl->mpReadBuffer = mpImpl->read(mpImpl->mnReadBufferLength, tmpNumBytes);
    if (tmpNumBytes != mpImpl->mnReadBufferLength)
        mpImpl->mnReadBufferLength = tmpNumBytes;

    mpImpl->mnReadBufferPos = 0;
    if (!mpImpl->mnReadBufferLength)
        return 0;

    numBytesRead = numBytes;

    mpImpl->mnReadBufferPos += numBytesRead;
    return mpImpl->mpReadBuffer;
}

long WPXSvInputStream::tell()
{
    long retVal = mpImpl->tell();
    return retVal - (long)mpImpl->mnReadBufferLength + (long)mpImpl->mnReadBufferPos;
}

int WPXSvInputStream::seek(long offset, librevenge::RVNG_SEEK_TYPE seekType)
{
    sal_Int64 tmpOffset = offset;
    if (seekType == librevenge::RVNG_SEEK_CUR)
        tmpOffset += tell();
    if (seekType == librevenge::RVNG_SEEK_END)
        tmpOffset += mpImpl->mnLength;

    int retVal = 0;
    if (tmpOffset < 0)
    {
        tmpOffset = 0;
        retVal = -1;
    }
    if (tmpOffset > mpImpl->mnLength)
    {
        tmpOffset = mpImpl->mnLength;
        retVal = -1;
    }

    if (tmpOffset < mpImpl->tell() && (unsigned long)tmpOffset >= (unsigned long)mpImpl->tell() - mpImpl->mnReadBufferLength)
    {
        mpImpl->mnReadBufferPos = (unsigned long)(tmpOffset + (long) mpImpl->mnReadBufferLength - (long) mpImpl->tell());
        return retVal;
    }

    mpImpl->invalidateReadBuffer();

    if (mpImpl->seek(tmpOffset))
        return -1;
    return retVal;
}

bool WPXSvInputStream::isEnd()
{
    return mpImpl->isEnd() && mpImpl->mnReadBufferPos == mpImpl->mnReadBufferLength;
}

bool WPXSvInputStream::isStructured()
{
    mpImpl->invalidateReadBuffer();
    return mpImpl->isStructured();
}

unsigned WPXSvInputStream::subStreamCount()
{
    mpImpl->invalidateReadBuffer();
    return mpImpl->subStreamCount();
}

const char *WPXSvInputStream::subStreamName(const unsigned id)
{
    mpImpl->invalidateReadBuffer();
    return mpImpl->subStreamName(id);
}

bool WPXSvInputStream::existsSubStream(const char *const name)
{
    mpImpl->invalidateReadBuffer();
    return mpImpl->existsSubStream(name);
}

librevenge::RVNGInputStream *WPXSvInputStream::getSubStreamByName(const char *name)
{
    mpImpl->invalidateReadBuffer();
    return mpImpl->getSubStreamByName(name);
}

librevenge::RVNGInputStream *WPXSvInputStream::getSubStreamById(const unsigned id)
{
    mpImpl->invalidateReadBuffer();
    return mpImpl->getSubStreamById(id);
}

}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
