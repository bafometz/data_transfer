#include "datatpackage.h"

struct crc32
{
    static void generate_table(uint32_t *table)
    {
        uint32_t polynomial = 0xEDB88320;
        for (uint32_t i = 0; i < tableSize; i++)
        {
            uint32_t c = i;
            for (size_t j = 0; j < 8; j++)
            {
                if (c & 1)
                {
                    c = polynomial ^ (c >> 1);
                }
                else
                {
                    c >>= 1;
                }
            }
            table[i] = c;
        }
    }

    static uint32_t update(uint32_t *table, uint32_t initial, const void *buf, size_t len)
    {
        uint32_t       c = initial ^ 0xFFFFFFFF;
        const uint8_t *u = static_cast< const uint8_t * >(buf);
        for (size_t i = 0; i < len; ++i)
        {
            c = table[(c ^ u[i]) & 0xFF] ^ (c >> 8);
        }
        return c ^ 0xFFFFFFFF;
    }

    const static int tableSize = 256;
};

std::vector< uint32_t > DatatPackage::table_(crc32::tableSize);

DatatPackage::DatatPackage() :
    data_(DatatPackage::maxDataSize() - DatatPackage::minSize())
{
}

DatatPackage::DatatPackage(DatatPackage &&dp) :
    packageCommand_ { std::move(dp.packageCommand_) },
    dataSize_ { std::move(dp.dataSize_) },
    data_ { std::move(dp.data_) },
    crc_ { std::move(dp.crc_) }
{
}

DatatPackage::DatatPackage(const std::vector< uint8_t > &data) :
    data_(DatatPackage::maxDataSize() - DatatPackage::minSize())
{
    replacePackage(data);
    calcChecksum();
}

DatatPackage::DatatPackage(const DatatPackage &dp) :
    packageCommand_ { dp.packageCommand_ },
    dataSize_ { dp.dataSize_ },
    data_(DatatPackage::maxDataSize() - DatatPackage::minSize()),
    crc_ { dp.crc_ }
{
    data_.insert(data_.begin(), dp.data_.begin(), dp.data_.end());
}

bool DatatPackage::verifyCheckSum()
{
    std::array< uint8_t, 4 > crc;
    calcCrc32(crc);

    for (int i = 0; i < 4; i++)
    {
        if (crc.at(i) != crc_.at(i))
        {
            return false;
        }
    }

    return true;
}

void DatatPackage::calcChecksum()
{
    calcCrc32(crc_);
}

void DatatPackage::setCommand(COMMAND cm)
{
    packageCommand_ = static_cast< uint8_t >(cm);
}

void DatatPackage::setData(uint16_t data)
{
    auto res = toBytes< std::vector< uint8_t > >(data);
    data_.insert(data_.begin(), res.begin(), res.end());
    dataSize_ = toBytes< std::array< uint8_t, 2 > >(( uint16_t )res.size());
}

void DatatPackage::setData(std::vector< uint8_t > &&data)
{
    data_     = std::move(data);
    dataSize_ = toBytes< std::array< uint8_t, 2 > >(( uint16_t )data_.size());
}

void DatatPackage::setData(const std::vector< uint8_t > &data, int size)
{
    data_.clear();
    if (size < 0)
    {
        data_.insert(data_.begin(), data.begin(), data.end());
        dataSize_ = toBytes< std::array< uint8_t, 2 > >(( uint16_t )data.size());
    }
    else
    {
        data_.insert(data_.begin(), data.begin(), data.begin() + size);
        dataSize_ = toBytes< std::array< uint8_t, 2 > >(( uint16_t )size);
    }
}

void DatatPackage::replacePackage(DatatPackage &&pkg)
{
    packageCommand_ = std::move(pkg.packageCommand_);
    dataSize_       = std::move(pkg.dataSize_);
    data_           = std::move(pkg.data_);
    crc_            = std::move(pkg.crc_);
}

void DatatPackage::replacePackage(const std::vector< uint8_t > &data)
{
    auto startPos = fillHeader(data);

    if (startPos == -1)
    {
        return;
    }

    auto offset = startPos + 4;
    auto size   = dataSizeFromHeader();
    data_.insert(data_.begin(), data.begin() + offset, data.begin() + offset + size);

    crc_.at(0) = *(data.begin() + size + offset);
    crc_.at(1) = *(data.begin() + size + offset + 1);
    crc_.at(2) = *(data.begin() + size + offset + 2);
    crc_.at(3) = *(data.begin() + size + offset + 3);
}

COMMAND DatatPackage::getCommand() const
{
    return static_cast< COMMAND >(packageCommand_);
}

int DatatPackage::getData(std::vector< uint8_t > &data) const
{
    const auto size = dataSizeFromHeader();
    data.clear();
    data.insert(data.begin(), data_.begin(), data_.begin() + size);
    return size;
}

int DatatPackage::getData() const
{
    if (data_.empty()) return 0;
    int ret = fromBytes< int >(data_);
    return ret;
}

void DatatPackage::clearData()
{
    dataSize_.at(0) = 0x00;
    dataSize_.at(1) = 0x00;
    data_.clear();
}

void DatatPackage::generateCrc32Table()
{
    crc32 crc;
    crc.generate_table(table_.data());
}

int DatatPackage::generatePackage(std::vector< uint8_t > &data) const
{
    auto dataSize = dataSizeFromHeader();
    data.reserve(sizeof(header_) + sizeof(packageCommand_) + sizeof(dataSize_) + crc_.size() + dataSize);
    data.clear();
    data.push_back(header_);
    data.push_back(packageCommand_);
    data.push_back(dataSize_[0]);
    data.push_back(dataSize_[1]);
    data.insert(data.end(), data_.begin(), data_.begin() + dataSize);
    data.insert(data.end(), crc_.begin(), crc_.end());
    return data.size();
}

uint16_t DatatPackage::dataSizeFromHeader() const
{
    return fromBytes< uint16_t >(dataSize_);
}

std::array< uint8_t, 4 > &DatatPackage::getCrc()
{
    return crc_;
}

uint16_t DatatPackage::maxDataSize()
{
    return UINT16_MAX;
}

uint64_t DatatPackage::maxSize()
{
    return UINT16_MAX + minSize();
}

uint16_t DatatPackage::minSize()
{
    return sizeof(header_) + sizeof(packageCommand_) + 2 + 4;
}

int DatatPackage::fillHeader(const std::vector< uint8_t > &data)
{
    int startPos = 0;
    int lastPos  = data.size() - 1;

    for (size_t i = 0; i < data.size(); i++)
    {
        if (data.at(i) == 0xAA)
        {
            startPos = i;
            break;
        }
    }

    if (startPos == lastPos) return -1;
    if (startPos + 3 >= lastPos)
    {
        return -2;
    }

    packageCommand_ = data.at(startPos + 1);
    dataSize_.at(0) = data.at(startPos + 2);
    dataSize_.at(1) = data.at(startPos + 3);
    return startPos;
}

void DatatPackage::calcCrc32(std::array< uint8_t, 4 > &result)
{
    // BE
    uint32_t crc = crc32::update(table_.data(), 0, &header_, sizeof(header_));
    crc          = crc32::update(table_.data(), crc, &packageCommand_, sizeof(packageCommand_));
    crc          = crc32::update(table_.data(), crc, dataSize_.data(), dataSize_.size());
    crc          = crc32::update(table_.data(), crc, data_.data(), dataSizeFromHeader());

    result.at(0) = (crc >> 24) & 0xFF;
    result.at(1) = (crc >> 16) & 0xFF;
    result.at(2) = (crc >> 8) & 0xFF;
    result.at(3) = crc & 0xFF;
}
