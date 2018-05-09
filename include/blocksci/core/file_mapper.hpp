//
//  file_mapper.hpp
//  blocksci
//
//  Created by Harry Kalodner on 7/23/17.
//
//

#ifndef file_mapper_hpp
#define file_mapper_hpp

#include <blocksci/blocksci_export.h>

#include <mio/mmap.hpp>

#include <array>
#include <cassert>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace blocksci {
    template<mio::access_mode mode = mio::access_mode::read>
    struct SimpleFileMapper;
    
    template <typename T, mio::access_mode mode = mio::access_mode::read>
    struct FixedSizeFileMapper;
    
    template <mio::access_mode mode = mio::access_mode::read, typename... T>
    struct IndexedFileMapper;
    
    template<typename U>
    struct add_const_ptr {
        using type = std::add_pointer_t<std::add_const_t<U>>;
    };
    
    template<typename... Types>
    struct add_const_ptr<std::tuple<Types...>> {
        using type = std::tuple<std::add_pointer_t<std::add_const_t<Types>>...>;
    };
    template<typename... Types>
    using add_const_ptr_t = typename add_const_ptr<Types...>::type;
    
    
    template<typename U>
    struct add_ptr {
        using type = std::add_pointer_t<U>;
    };
    
    template<typename... Types>
    struct add_ptr<std::tuple<Types...>> {
        using type = std::tuple<std::add_pointer_t<Types>...>;
    };
    template<typename... Types>
    using add_ptr_t = typename add_ptr<Types...>::type;
    
    using OffsetType = int64_t;
    constexpr OffsetType InvalidFileIndex = std::numeric_limits<OffsetType>::max();

    struct BLOCKSCI_EXPORT FileInfo {
        std::string path;
        
        FileInfo(std::string path_) : path(std::move(path_)) {}
        
        bool exists() const;
        
        OffsetType size() const;
        
        void resize(OffsetType offset);
        
        void create(OffsetType offset);
    };
    
    template <mio::access_mode mode>
    struct BLOCKSCI_EXPORT SimpleFileMapperBase {
    protected:
        mio::basic_mmap<mode, char> file;
    public:
        FileInfo fileInfo;
        
        SimpleFileMapperBase(const std::string &path_) : fileInfo(path_ + ".dat") {
            openFile();
        }
        
        void openFile() {
            std::error_code error;
            file.map(fileInfo.path, 0, mio::map_entire_file, error);
            if(error) {
                throw error;
            }
        }
        
        bool isGood() const {
            return file.is_open();
        }
        
        const char *getDataAtOffset(OffsetType offset) const {
            if (offset == InvalidFileIndex) {
                return nullptr;
            }
            assert(offset < size());
            return &file[offset];
        }
        
        OffsetType size() const {
            return file.length();
        }
        
        void reload() {
            if (fileInfo.exists()) {
                if (!file.is_open() || fileInfo.size() != file.size()) {
                    openFile();
                }
            } else {
                if (file.is_open()) {
                    file.unmap();
                }
            }
        }
    };
    
    template <typename MainType>
    class ArbitraryLengthData {
        std::vector<char> rawData;
        
    public:
        explicit ArbitraryLengthData(const MainType &head) {
            rawData.reserve(head.realSize());
            add(head);
        }
        
        template <typename T>
        void add(const T &t) {
            auto data = reinterpret_cast<const char *>(&t);
            rawData.insert(rawData.end(), data, data + sizeof(t));
        }
        
        template <typename It>
        void add(It begin, It end) {
            for (; begin != end; ++begin) {
                add(*begin);
            }
        }
        
        OffsetType size() const {
            return static_cast<OffsetType>(rawData.size());
        }
        
        const char *finalize() {
            auto charsToAdd = rawData.size() % alignof(MainType);
            if (charsToAdd > 0) {
                for (size_t i = 0; i < (alignof(MainType) - charsToAdd); i++) {
                    rawData.push_back(0);
                }
            }
            return rawData.data();
        }
    };

    template <>
    struct BLOCKSCI_EXPORT SimpleFileMapper<mio::access_mode::read> : public SimpleFileMapperBase<mio::access_mode::read> {
        using SimpleFileMapperBase::SimpleFileMapperBase;
    };
    
    template <>
    struct BLOCKSCI_EXPORT SimpleFileMapper<mio::access_mode::write> : public SimpleFileMapperBase<mio::access_mode::write> {
        static constexpr size_t maxBufferSize = 50000000;
        std::vector<char> buffer;
        static constexpr auto mode = mio::access_mode::read;
        
        SimpleFileMapper(const std::string &path) : SimpleFileMapperBase(path), writePos(size()) {}

        ~SimpleFileMapper() {
            clearBuffer();
        }
        
    private:
        OffsetType writePos;
        
        char *getWritePos() {
            auto fileEnd = file.size();
            if (writePos < fileEnd) {
                return &file[writePos];
            } else if (writePos < fileEnd + static_cast<OffsetType>(buffer.size())) {
                return reinterpret_cast<char *>(buffer.data()) + (writePos - fileEnd);
            } else {
                return nullptr;
            }
        }
        
        
       OffsetType writeSpace() const {
            auto fileEnd = file.size();
            if (writePos < fileEnd) {
                return fileEnd - writePos;
            } else if (writePos < fileEnd + static_cast<OffsetType>(buffer.size())) {
                return (writePos - fileEnd) - static_cast<OffsetType>(buffer.size());
            } else {
                return 0;
            }
        }
        
    public:
        
        void reload() {
            // Triggers a reload of the file as well
            clearBuffer();
        }
        
        OffsetType getWriteOffset() const {
            return writePos;
        }
        
        bool write(const char *valuePos, OffsetType amountToWrite) {
            auto fileEnd = file.size();
            if (writePos < fileEnd) {
                auto writeAmount = std::min(amountToWrite, writeSpace());
                assert(writeAmount > 0);
                memcpy(getWritePos(), valuePos, static_cast<size_t>(writeAmount));
                amountToWrite -= writeAmount;
                writePos += writeAmount;
                valuePos += writeAmount;
                if (amountToWrite == 0) {
                    return false;
                }
            }
            
            if (writePos >= fileEnd && writePos < static_cast<OffsetType>(buffer.size())) {
                auto writeAmount = std::min(amountToWrite, writeSpace());
                memcpy(getWritePos(), valuePos, static_cast<size_t>(writeAmount));
                amountToWrite -= writeAmount;
                writePos += writeAmount;
                valuePos += writeAmount;
                
                if (amountToWrite == 0) {
                    return false;
                }
            }
            
            assert(writePos == fileEnd + buffer.size());
            
            buffer.insert(buffer.end(), valuePos, valuePos + amountToWrite);
            writePos += amountToWrite;
            bool bufferFull = buffer.size() > maxBufferSize;
            if (bufferFull) {
                clearBuffer();
            }
            return bufferFull;
        }
        
        template<typename T, typename = std::enable_if_t<std::is_trivially_copyable<T>::value>>
        bool write(const T &t) {
            return write(reinterpret_cast<const char *>(&t), sizeof(t));
        }
        
        template<typename T>
        bool write(const ArbitraryLengthData<T> &t) {
            return write(t.dataView(), t.size());
        }
        
        void clearBuffer();
        
        char *getDataAtOffset(OffsetType offset) {
            auto fileEnd = file.size();
            assert(offset < fileEnd + buffer.size() || offset == InvalidFileIndex);
            if (offset == InvalidFileIndex) {
                return nullptr;
            } else if (offset < fileEnd) {
                return &file[offset];
            } else {
                return buffer.data() + (offset - fileEnd);
            }
        }
        
        const char *getDataAtOffset(OffsetType offset) const {
            auto fileEnd = file.size();
            assert(offset < fileEnd + buffer.size() || offset == InvalidFileIndex);
            if (offset == InvalidFileIndex) {
                return nullptr;
            } else if (offset < fileEnd) {
                return &file[offset];
            } else {
                return buffer.data() + (offset - fileEnd);
            }
        }
        
        OffsetType size() const {
            return SimpleFileMapperBase::size() + static_cast<OffsetType>(buffer.size());
        }
        
        void seekEnd() {
            writePos = file.size() + static_cast<OffsetType>(buffer.size());
        }
        
        void seek(OffsetType offset) {
            writePos = offset;
        }
        
        void truncate(OffsetType offset);
    };
    
    template <typename T, mio::access_mode mode>
    struct BLOCKSCI_EXPORT FixedSizeFileMapper {
    private:
        SimpleFileMapper<mode> dataFile;
        OffsetType getPos(OffsetType index) const {
            return index * static_cast<OffsetType>(sizeof(T));
        }
        
    public:
        using size_type = uint64_t;
        using difference_type = std::ptrdiff_t;
        using pointer = add_ptr_t<T>;
        using const_pointer = add_const_ptr_t<T>;

        
        explicit FixedSizeFileMapper(std::string path) : dataFile(std::move(path)) {}
        
        const_pointer operator[](OffsetType index) const {
            assert(index < size());
            const char *pos = dataFile.getDataAtOffset(getPos(index));
            return reinterpret_cast<const_pointer>(pos);
        }
        
        template <typename Dummy = void, typename Dummy2 = std::enable_if_t<mode == mio::access_mode::write, Dummy>>
        pointer operator[](OffsetType index) {
            assert(index < size());
            char *pos = dataFile.getDataAtOffset(getPos(index));
            return reinterpret_cast<pointer>(pos);
        }

        const_pointer getDataAtIndex(OffsetType index) const {
            return (*this)[index];
        }
        
        pointer getDataAtIndex(OffsetType index) {
            return (*this)[index];
        }

        void seekEnd() {
            dataFile.seekEnd();
        }
        
        void seek(OffsetType index) {
            dataFile.seek(getPos(index));
        }
        
        void reload() {
            dataFile.reload();
        }
        
        void clearBuffer() {
            dataFile.clearBuffer();
        }
        
        bool write(const T &t) {
            return dataFile.write(t);
        }
        
        OffsetType size() const {
            constexpr OffsetType unitSize = sizeof(T);
            return dataFile.size() / unitSize;
        }
        
        OffsetType fileSize() const {
            return dataFile.fileSize() / sizeof(T);
        }
        
        void truncate(OffsetType index) {
            dataFile.truncate(getPos(index));
        }
        
        template<typename Test>
        std::vector<uint32_t> findAll(Test test) const {
            auto itemCount = size();
            std::vector<uint32_t> indexes;
            uint32_t index = 0;
            while (index < itemCount) {
                auto nextItem = *this(index);
                if (test(*nextItem)) {
                    indexes.push_back(index);
                }
                index++;
            }
            return indexes;
        }
    };
    
    template <size_t indexCount>
    using FileIndex = std::array<OffsetType, indexCount>;
    
    
    template <typename Tup, typename A, std::size_t ...I>
    Tup tuple_cast_helper(A t1, std::index_sequence<I...>) {
        return std::make_tuple( reinterpret_cast<std::tuple_element_t<I,Tup>>(std::get<I>(t1))...);
    }
    
    template <typename ...A, size_t N>
    add_const_ptr_t<std::tuple<A...>> tuple_cast(std::array<const char *, N> t1) {
        static_assert(sizeof...(A) == N, "The tuple sizes must be the same");
        return tuple_cast_helper<add_const_ptr_t<std::tuple<A...>>>( t1, std::make_index_sequence<N>{} );
    }
    
    template <typename ...A, size_t N>
    add_ptr_t<std::tuple<A...>> tuple_cast(std::array<char *, N> t1) {
        static_assert(sizeof...(A) == N, "The tuple sizes must be the same");
        return tuple_cast_helper<add_ptr_t<std::tuple<A...>>>( t1, std::make_index_sequence<N>{} );
    }
    
    template <mio::access_mode mode, typename... T>
    struct BLOCKSCI_EXPORT IndexedFileMapper {
    private:
        template<size_t indexNum>
        using nth_element = std::tuple_element_t<indexNum, std::tuple<T...>>;
        static constexpr size_t indexCount = sizeof...(T);
        SimpleFileMapper<mode> dataFile;
        FixedSizeFileMapper<FileIndex<indexCount>, mode> indexFile;
        
        void writeNewImp(const char *valuePos, OffsetType amountToWrite) {
            assert(amountToWrite % alignof(nth_element<0>) == 0);
            FileIndex<indexCount> fileIndex;
            fileIndex[0] = dataFile.getWriteOffset();
            for(size_t i = 1; i < indexCount; i++) {
                fileIndex[i] = InvalidFileIndex;
            }
            indexFile.write(fileIndex);
            dataFile.write(valuePos, amountToWrite);
        }
        
        template<size_t indexNum>
        void writeUpdateImp(uint32_t addressNum, const char *valuePos, OffsetType amountToWrite) {
            static_assert(indexNum > 0, "Must call write without addressNum for first element");
            static_assert(indexNum < indexCount, "Index out of range");
            assert(amountToWrite % alignof(nth_element<indexNum>) == 0);
            auto &fileIndex = *indexFile[addressNum];
            fileIndex[indexNum] = dataFile.getWriteOffset();
            dataFile.write(valuePos, amountToWrite);
            
        }
        
        template<size_t indexNum = 0>
        OffsetType getOffset(uint32_t index) const {
            static_assert(indexNum < sizeof...(T), "Trying to fetch index out of bounds");
            auto indexData = indexFile[index];
            auto offset = (*indexData)[indexNum];
            assert(offset < dataFile.size() || offset == InvalidFileIndex);
            return offset;
        }
        
    public:
        explicit IndexedFileMapper(const std::string &pathPrefix) : dataFile(pathPrefix + "_data"), indexFile(pathPrefix + "_index") {
        }
        
        void reload() {
            indexFile.reload();
            dataFile.reload();
        }
        
        void clearBuffer() {
            indexFile.clearBuffer();
            dataFile.clearBuffer();
        }
        
        FileIndex<sizeof...(T)> getOffsets(uint32_t index) const {
            return *indexFile[index];
        }
        
        OffsetType size() const {
            return indexFile.size();
        }
        
        OffsetType fileSize() const {
            return indexFile.fileSize();
        }
        
        void truncate(uint32_t index) {
            if (index < size()) {
                auto offsets = getOffsets(index);
                indexFile.truncate(index);
                dataFile.truncate(offsets[0]);
            }
        }
        
        void seekEnd() {
            indexFile.seekEnd();
            dataFile.seekEnd();
        }
        
        void seek(uint32_t index, OffsetType dataOffset) {
            indexFile.seek(index);
            dataFile.seek(dataOffset);
        }

        void grow(uint32_t indexSize, OffsetType dataSize) {
            indexFile.truncate(indexFile.size() + indexSize);
            dataFile.truncate(dataFile.size() + dataSize);
        }
        
        void write(const nth_element<0> &t) {
            writeNewImp(&t, sizeof(t));
        }
        
        void write(ArbitraryLengthData<nth_element<0>> &t) {
            auto finalData = t.finalize();
            writeNewImp(finalData, t.size());
        }
        
        template<size_t indexNum>
        void write(uint32_t addressNum, const nth_element<indexNum> &t) {
            writeUpdateImp<indexNum>(addressNum, &t, sizeof(t));
        }
        
        template<size_t indexNum>
        void write(uint32_t addressNum, ArbitraryLengthData<nth_element<indexNum>> &t) {
            auto finalData = t.finalize();
            writeUpdateImp<indexNum>(addressNum, finalData, t.size());
        }
        
        template<size_t indexNum = 0>
        add_const_ptr_t<nth_element<indexNum>> getDataAtIndex(uint32_t index) const {
            assert(index < size());
            auto offset = getOffset<indexNum>(index);
            auto pointer = dataFile.getDataAtOffset(offset);
            return reinterpret_cast<add_const_ptr_t<nth_element<indexNum>>>(pointer);
        }
        
        template<size_t indexNum = 0>
        add_ptr_t<nth_element<indexNum>> getDataAtIndex(uint32_t index) {
            assert(index < size());
            auto offset = getOffset<indexNum>(index);
            auto pointer = dataFile.getDataAtOffset(offset);
            return reinterpret_cast<add_ptr_t<nth_element<indexNum>>>(pointer);
        }
        
        template <size_t Z = sizeof...(T)>
        auto getData(std::enable_if_t<(Z > 1), uint32_t> index) const {
            assert(index < size());
            auto offsets = getOffsets(index);
            std::array<const char *, sizeof...(T)> pointers;
            for (size_t i = 0; i < sizeof...(T); i++) {
                pointers[i] = dataFile.getDataAtOffset(offsets[i]);
            }
            return tuple_cast<T...>(pointers);
        }
        
        template <size_t Z = sizeof...(T)>
        auto getData(std::enable_if_t<(Z == 1), uint32_t> index) const {
            assert(index < size());
            return getDataAtIndex<0>(index);
        }
        
        template <size_t Z = sizeof...(T)>
        auto getData(std::enable_if_t<(Z > 1), uint32_t> index) {
            assert(index < size());
            auto offsets = getOffsets(index);
            std::array<char *, sizeof...(T)> pointers;
            for (size_t i = 0; i < sizeof...(T); i++) {
                pointers[i] = dataFile.getDataAtOffset(offsets[i]);
            }
            return tuple_cast<T...>(pointers);
        }
        
        template <size_t Z = sizeof...(T)>
        auto getData(std::enable_if_t<(Z <= 1), uint32_t> index) {
            assert(index < size());
            return getDataAtIndex<0>(index);
        }

        auto operator[](uint32_t index) const {
            return getData(index);
        }

        auto operator[](uint32_t index) {
            return getData(index);
        }
    };
}

#endif /* file_mapper_hpp */
