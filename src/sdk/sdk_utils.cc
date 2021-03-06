// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: Xu Peilin (xupeilin@baidu.com)

#include "sdk/sdk_utils.h"

#include <iostream>

#include "common/base/string_ext.h"
#include "common/base/string_number.h"
#include "common/file/file_path.h"
#include "gflags/gflags.h"
#include "glog/logging.h"

#include "sdk/filter_utils.h"
DECLARE_int64(tera_tablet_write_block_size);
DECLARE_int64(tera_tablet_ldb_sst_size);
DECLARE_int64(tera_master_merge_tablet_size);

namespace tera {

string LgProp2Str(bool type) {
    if (type) {
        return "snappy";
    } else {
        return "none";
    }
}

string LgProp2Str(StoreMedium type) {
    if (type == DiskStore) {
        return "disk";
    } else if (type == FlashStore) {
        return "flash";
    } else if (type == MemoryStore) {
        return "memory";
    } else {
        return "";
    }
}

string TableProp2Str(RawKey type) {
    if (type == Readable) {
        return "readable";
    } else if (type == Binary) {
        return "binary";
    } else if (type == TTLKv) {
        return "ttlkv";
    } else {
        return "";
    }
}

void ShowTableSchema(const TableSchema& schema, bool is_x) {
    std::stringstream ss;
    if (schema.kv_only()) {
        const LocalityGroupSchema& lg_schema = schema.locality_groups(0);
        ss << "\n  " << schema.name() << " <";
        if (is_x || schema.raw_key() != Readable) {
            ss << "rawkey=" << TableProp2Str(schema.raw_key()) << ",";
        }
        ss << "splitsize=" << schema.split_size() << ",";
        if (is_x || schema.merge_size() != FLAGS_tera_master_merge_tablet_size) {
            ss << "mergesize=" << schema.merge_size() << ",";
        }
        if (is_x || lg_schema.store_type() != DiskStore) {
            ss << "storage=" << LgProp2Str(lg_schema.store_type()) << ",";
        }
        if (is_x || lg_schema.block_size() != FLAGS_tera_tablet_write_block_size) {
            ss << "blocksize=" << lg_schema.block_size() << ",";
        }
        ss << "\b>\n" << "  (kv mode)\n";
        std::cout << ss.str() << std::endl;
        return;
    }

    ss << "\n  " << schema.name() << " <";
    if (is_x || schema.raw_key() != Readable) {
        ss << "rawkey=" << TableProp2Str(schema.raw_key()) << ",";
    }
    ss << "splitsize=" << schema.split_size() << ",";
    if (is_x || schema.merge_size() != FLAGS_tera_master_merge_tablet_size) {
        ss << "mergesize=" << schema.merge_size() << ",";
    }
    ss << "\b> {" << std::endl;

    size_t lg_num = schema.locality_groups_size();
    size_t cf_num = schema.column_families_size();
    for (size_t lg_no = 0; lg_no < lg_num; ++lg_no) {
        const LocalityGroupSchema& lg_schema = schema.locality_groups(lg_no);
        ss << "      " << lg_schema.name() << " <";
        ss << "storage=" << LgProp2Str(lg_schema.store_type()) << ",";
        if (is_x || lg_schema.block_size() != FLAGS_tera_tablet_write_block_size) {
            ss << "blocksize=" << lg_schema.block_size() << ",";
        }
        if (is_x || lg_schema.sst_size() != FLAGS_tera_tablet_ldb_sst_size) {
            ss << "sst_size=" << (lg_schema.sst_size() >> 20) << ",";
        }
        if (lg_schema.use_memtable_on_leveldb()) {
            ss << "use_memtable_on_leveldb="
                << lg_schema.use_memtable_on_leveldb()
                << ",memtable_ldb_write_buffer_size="
                << lg_schema.memtable_ldb_write_buffer_size()
                << ",memtable_ldb_block_size="
                << lg_schema.memtable_ldb_block_size() << ",";
        }
        ss << "\b> {" << std::endl;
        for (size_t cf_no = 0; cf_no < cf_num; ++cf_no) {
            const ColumnFamilySchema& cf_schema = schema.column_families(cf_no);
            if (cf_schema.locality_group() != lg_schema.name()) {
                continue;
            }
            ss << "          " << cf_schema.name();
            std::stringstream cf_ss;
            cf_ss << " <";
            if (is_x || cf_schema.max_versions() != 1) {
                cf_ss << "maxversions=" << cf_schema.max_versions() << ",";
            }
            if (is_x || cf_schema.min_versions() != 1) {
                cf_ss << "minversions=" << cf_schema.min_versions() << ",";
            }
            if (is_x || cf_schema.time_to_live() != 0) {
                cf_ss << "ttl=" << cf_schema.time_to_live() << ",";
            }
            if (is_x || cf_schema.type() != "") {
                cf_ss << "type=" << cf_schema.type() << ",";
            }
            cf_ss << "\b>";
            if (cf_ss.str().size() > 5) {
                ss << cf_ss.str();
            }
            ss << "," << std::endl;
        }
        ss << "      }," << std::endl;
    }
    ss << "  }" << std::endl;
    std::cout << ss.str() << std::endl;
}

void ShowTableMeta(const TableMeta& meta) {
    const TableSchema& schema = meta.schema();
    ShowTableSchema(schema);
    std::cout << "Snapshot:" << std::endl;
    for (int32_t i = 0; i < meta.snapshot_list_size(); ++i) {
        std::cout << " " << meta.snapshot_list(i) << std::endl;
    }
    std::cout << std::endl;
}

void ShowTableDescriptor(TableDescriptor& table_desc, bool is_x) {
    TableSchema schema;
    TableDescToSchema(table_desc, &schema);
    ShowTableSchema(schema, is_x);
}

void TableDescToSchema(const TableDescriptor& desc, TableSchema* schema) {
    schema->set_name(desc.TableName());
    switch (desc.RawKey()) {
        case kBinary:
            schema->set_raw_key(Binary);
            break;
        case kTTLKv:
            schema->set_raw_key(TTLKv);
            break;
        default:
            schema->set_raw_key(Readable);
            break;
    }
    schema->set_split_size(desc.SplitSize());
    schema->set_merge_size(desc.MergeSize());
    schema->set_kv_only(desc.IsKv());

    // add lg
    int num = desc.LocalityGroupNum();
    for (int i = 0; i < num; ++i) {
        LocalityGroupSchema* lg = schema->add_locality_groups();
        const LocalityGroupDescriptor* lgdesc = desc.LocalityGroup(i);
        lg->set_block_size(lgdesc->BlockSize());
        lg->set_compress_type(lgdesc->Compress() != kNoneCompress);
        lg->set_name(lgdesc->Name());
        // printf("add lg %s\n", lgdesc->Name().c_str());
        switch (lgdesc->Store()) {
            case kInMemory:
                lg->set_store_type(MemoryStore);
                break;
            case kInFlash:
                lg->set_store_type(FlashStore);
                break;
            default:
                lg->set_store_type(DiskStore);
                break;
        }
        lg->set_use_memtable_on_leveldb(lgdesc->UseMemtableOnLeveldb());
        if (lgdesc->MemtableLdbBlockSize() > 0) {
            lg->set_memtable_ldb_write_buffer_size(lgdesc->MemtableLdbWriteBufferSize());
            lg->set_memtable_ldb_block_size(lgdesc->MemtableLdbBlockSize());
        }
        lg->set_sst_size(lgdesc->SstSize());
        lg->set_id(lgdesc->Id());
    }
    // add cf
    int cfnum = desc.ColumnFamilyNum();
    for (int i = 0; i < cfnum; ++i) {
        ColumnFamilySchema* cf = schema->add_column_families();
        const ColumnFamilyDescriptor* cf_desc = desc.ColumnFamily(i);
        const LocalityGroupDescriptor* lg_desc =
            desc.LocalityGroup(cf_desc->LocalityGroup());
        assert(lg_desc);
        cf->set_name(cf_desc->Name());
        cf->set_time_to_live(cf_desc->TimeToLive());
        cf->set_locality_group(cf_desc->LocalityGroup());
        cf->set_max_versions(cf_desc->MaxVersions());
        cf->set_min_versions(cf_desc->MinVersions());
        cf->set_type(cf_desc->Type());
    }
}

void TableSchemaToDesc(const TableSchema& schema, TableDescriptor* desc) {
    if (schema.kv_only()) {
        desc->SetKvOnly();
    }

    switch (schema.raw_key()) {
        case Binary:
            desc->SetRawKey(kBinary);
            break;
        default:
            desc->SetRawKey(kReadable);
    }

    if (schema.has_split_size()) {
        desc->SetSplitSize(schema.split_size());
    }
    if (schema.has_merge_size()) {
        desc->SetMergeSize(schema.merge_size());
    }

    int32_t lg_num = schema.locality_groups_size();
    for (int32_t i = 0; i < lg_num; i++) {
        const LocalityGroupSchema& lg = schema.locality_groups(i);
        LocalityGroupDescriptor* lgd = desc->AddLocalityGroup(lg.name());
        if (lgd == NULL) {
            continue;
        }
        lgd->SetBlockSize(lg.block_size());
        switch (lg.store_type()) {
            case MemoryStore:
                lgd->SetStore(kInMemory);
                break;
            case FlashStore:
                lgd->SetStore(kInFlash);
                break;
            default:
                lgd->SetStore(kInDisk);
                break;
        }
        lgd->SetCompress(lg.compress_type() ? kSnappyCompress : kNoneCompress);
        lgd->SetUseBloomfilter(lg.use_bloom_filter());
        lgd->SetUseMemtableOnLeveldb(lg.use_memtable_on_leveldb());
        lgd->SetMemtableLdbWriteBufferSize(lg.memtable_ldb_write_buffer_size());
        lgd->SetMemtableLdbBlockSize(lg.memtable_ldb_block_size());
        lgd->SetSstSize(lg.sst_size());
    }
    int32_t cf_num = schema.column_families_size();
    for (int32_t i = 0; i < cf_num; i++) {
        const ColumnFamilySchema& cf = schema.column_families(i);
        ColumnFamilyDescriptor* cfd =
            desc->AddColumnFamily(cf.name(), cf.locality_group());
        if (cfd == NULL) {
            continue;
        }
        cfd->SetDiskQuota(cf.disk_quota());
        cfd->SetMaxVersions(cf.max_versions());
        cfd->SetMinVersions(cf.min_versions());
        cfd->SetTimeToLive(cf.time_to_live());
        cfd->SetType(cf.type());
    }
}

bool SetCfProperties(const PropertyList& props, ColumnFamilyDescriptor* desc) {
    for (size_t i = 0; i < props.size(); ++i) {
        const Property& prop = props[i];
        if (prop.first == "ttl") {
            int32_t ttl = atoi(prop.second.c_str());
            if (ttl < 0) {
                LOG(ERROR) << "illegal value: " << prop.second
                    << " for property: " << prop.first;
                return false;
            }
            desc->SetTimeToLive(ttl);
        } else if (prop.first == "maxversions") {
            int32_t versions = atol(prop.second.c_str());
            if (versions <= 0) {
                LOG(ERROR) << "illegal value: " << prop.second
                    << " for property: " << prop.first;
                return false;
            }
            desc->SetMaxVersions(versions);
        } else if (prop.first == "minversions") {
            int32_t versions = atol(prop.second.c_str());
            if (versions <= 0) {
                LOG(ERROR) << "illegal value: " << prop.second
                    << " for property: " << prop.first;
                return false;
            }
            desc->SetMinVersions(versions);
        } else if (prop.first == "diskquota") {
            int64_t quota = atol(prop.second.c_str());
            if (quota <= 0) {
                LOG(ERROR) << "illegal value: " << prop.second
                    << " for property: " << prop.first;
                return false;
            }
            desc->SetDiskQuota(quota);
        } else {
            LOG(ERROR) << "illegal cf props: " << prop.first;
            return false;
        }
    }
    return true;
}

bool SetLgProperties(const PropertyList& props, LocalityGroupDescriptor* desc) {
    for (size_t i = 0; i < props.size(); ++i) {
        const Property& prop = props[i];
        if (prop.first == "compress") {
            if (prop.second == "none") {
                desc->SetCompress(kNoneCompress);
            } else if (prop.second == "snappy") {
                desc->SetCompress(kSnappyCompress);
            } else {
                LOG(ERROR) << "illegal value: " << prop.second
                    << " for property: " << prop.first;
                return false;
            }
        } else if (prop.first == "storage") {
            if (prop.second == "disk") {
                desc->SetStore(kInDisk);
            } else if (prop.second == "flash") {
                desc->SetStore(kInFlash);
            } else if (prop.second == "memory") {
                desc->SetStore(kInMemory);
            } else {
                LOG(ERROR) << "illegal value: " << prop.second
                    << " for property: " << prop.first;
                return false;
            }
        } else if (prop.first == "blocksize") {
            int blocksize = atoi(prop.second.c_str());
            if (blocksize <= 0) {
                LOG(ERROR) << "illegal value: " << prop.second
                    << " for property: " << prop.first;
                return false;
            }
            desc->SetBlockSize(blocksize);
        } else if (prop.first == "use_memtable_on_leveldb") {
            if (prop.second == "true") {
                desc->SetUseMemtableOnLeveldb(true);
            } else if (prop.second == "false") {
                desc->SetUseMemtableOnLeveldb(false);
            } else {
                LOG(ERROR) << "illegal value: " << prop.second
                           << " for property: " << prop.first;
                return false;
            }
        } else if (prop.first == "memtable_ldb_write_buffer_size") {
            int32_t buffer_size = atoi(prop.second.c_str()); //MB
            if (buffer_size <= 0) {
                LOG(ERROR) << "illegal value: " << prop.second
                           << " for property: " << prop.first;
                return false;
            }
            desc->SetMemtableLdbWriteBufferSize(buffer_size);
        } else if (prop.first == "memtable_ldb_block_size") {
            int32_t block_size = atoi(prop.second.c_str()); //KB
            if (block_size <= 0) {
                LOG(ERROR) << "illegal value: " << prop.second
                           << " for property: " << prop.first;
                return false;
            }
            desc->SetMemtableLdbBlockSize(block_size);
        } else if (prop.first == "sst_size") {
            int32_t sst_size = atoi(prop.second.c_str());
            if (sst_size <= 0) {
                LOG(ERROR) << "illegal value: " << prop.second
                           << " for property: " << prop.first;
                return false;
            }
            desc->SetSstSize(sst_size);
        } else {
            LOG(ERROR) << "illegal lg property: " << prop.first;
            return false;
        }
    }
    return true;
}

bool SetTableProperties(const PropertyList& props, TableDescriptor* desc) {
    for (size_t i = 0; i < props.size(); ++i) {
        const Property& prop = props[i];
        if (prop.first == "rawkey") {
            if (prop.second == "readable") {
                desc->SetRawKey(kReadable);
            } else if (prop.second == "binary") {
                desc->SetRawKey(kBinary);
            } else if (prop.second == "ttlkv") {
                desc->SetRawKey(kTTLKv);
            } else {
                LOG(ERROR) << "illegal value: " << prop.second
                    << " for property: " << prop.first;
                return false;
            }
        } else if (prop.first == "splitsize") {
            int splitsize = atoi(prop.second.c_str());
            if (splitsize < 0) { // splitsize == 0 : split closed
                LOG(ERROR) << "illegal value: " << prop.second
                    << " for property: " << prop.first;
                return false;
            }
            desc->SetSplitSize(splitsize);
        } else if (prop.first == "mergesize") {
            int mergesize = atoi(prop.second.c_str());
            if (mergesize < 0) { // mergesize == 0 : merge closed
                LOG(ERROR) << "illegal value: " << prop.second
                    << " for property: " << prop.first;
                return false;
            }
            desc->SetMergeSize(mergesize);
        } else {
            LOG(ERROR) << "illegal table property: " << prop.first;
            return false;
        }
    }
    return true;
}
bool SetCfProperties(const string& name, const string& value,
                     ColumnFamilyDescriptor* desc) {
    if (name == "ttl") {
        int32_t ttl = atoi(value.c_str());
        if (ttl < 0){
            return false;
        }
        desc->SetTimeToLive(ttl);
    } else if (name == "maxversions") {
        int32_t versions = atol(value.c_str());
        if (versions <= 0){
            return false;
        }
        desc->SetMaxVersions(versions);
    } else if (name == "minversions") {
        int32_t versions = atol(value.c_str());
        if (versions <= 0){
            return false;
        }
        desc->SetMinVersions(versions);
    } else if (name == "diskquota") {
        int64_t quota = atol(value.c_str());
        if (quota <= 0){
            return false;
        }
        desc->SetDiskQuota(quota);
    } else if (name == "type") {
        desc->SetType(value);
    }else {
        return false;
    }
    return true;
}

bool SetLgProperties(const string& name, const string& value,
                     LocalityGroupDescriptor* desc) {
    if (name == "compress") {
        if (value == "none") {
            desc->SetCompress(kNoneCompress);
        } else if (value == "snappy") {
            desc->SetCompress(kSnappyCompress);
        } else {
            return false;
        }
    } else if (name == "storage") {
        if (value == "disk") {
            desc->SetStore(kInDisk);
        } else if (value == "flash") {
            desc->SetStore(kInFlash);
        } else if (value == "memory") {
            desc->SetStore(kInMemory);
        } else {
            return false;
        }
    } else if (name == "blocksize") {
        int blocksize = atoi(value.c_str());
        if (blocksize <= 0){
            return false;
        }
        desc->SetBlockSize(blocksize);
    } else if (name == "use_memtable_on_leveldb") {
        if (value == "true") {
            desc->SetUseMemtableOnLeveldb(true);
        } else if (value == "false") {
            desc->SetUseMemtableOnLeveldb(false);
        } else {
            return false;
        }
    } else if (name == "memtable_ldb_write_buffer_size") {
        int32_t buffer_size = atoi(value.c_str()); //MB
        if (buffer_size <= 0) {
            return false;
        }
        desc->SetMemtableLdbWriteBufferSize(buffer_size);
    } else if (name == "memtable_ldb_block_size") {
        int32_t block_size = atoi(value.c_str()); //KB
        if (block_size <= 0) {
            return false;
        }
        desc->SetMemtableLdbBlockSize(block_size);
    } else if (name == "sst_size") {
        int32_t sst_size = atoi(value.c_str());
        if (sst_size <= 0) {
            return false;
        }
        desc->SetSstSize(sst_size);
    } else {
        return false;
    }
    return true;
}

bool SetTableProperties(const string& name, const string& value,
                        TableDescriptor* desc) {
    if (name == "rawkey") {
        if (value == "readable") {
            desc->SetRawKey(kReadable);
        } else if (value == "binary") {
            desc->SetRawKey(kBinary);
        } else if (value == "ttlkv") {
            desc->SetRawKey(kTTLKv);
        } else {
            return false;
        }
    } else if (name == "splitsize") {
        int splitsize = atoi(value.c_str());
        if (splitsize < 0) { // splitsize == 0 : split closed
            return false;
        }
        desc->SetSplitSize(splitsize);
    } else if (name == "mergesize") {
        int mergesize = atoi(value.c_str());
        if (mergesize < 0) { // mergesize == 0 : merge closed
            return false;
        }
        desc->SetMergeSize(mergesize);
    } else {
        return false;
    }
    return true;
}

//   prefix:property=value
bool ParsePrefixPropertyValue(const string& pair, string& prefix, string& property, string& value) {
    string::size_type i = pair.find(":");
    string::size_type k = pair.find("=");
    if (i == string::npos || k == string::npos ||
        i == 0 || i + 1 >= k || k == pair.length() - 1) {
        return false;
    }
    prefix = pair.substr(0, i);
    property = pair.substr(i + 1, k - i - 1);
    value = pair.substr(k + 1, pair.length() - 1);
    return true;
}

string PrefixType(const std::string& property) {
    string lg_prop[] = {
        "compress", "storage", "blocksize", "use_memtable_on_leveldb",
        "memtable_ldb_write_buffer_size", "memtable_ldb_block_size", "sst_size"};
    string cf_prop[] = {"ttl", "maxversions", "minversions", "diskquota"};

    std::set<string> lgset(lg_prop, lg_prop + sizeof(lg_prop) / sizeof(lg_prop[0]));
    std::set<string> cfset(cf_prop, cf_prop + sizeof(cf_prop) / sizeof(cf_prop[0]));
    if (lgset.find(property) != lgset.end()) {
        return string("lg");
    } else if (cfset.find(property) != cfset.end()) {
        return string("cf");
    }
    return string("unknown");
}

bool HasInvalidCharInSchema(const string& schema) {
    for (size_t i = 0; i < schema.length(); i++) {
        char ch = schema[i];
        if (isalnum(ch) || ch == '_' || ch == ':' || ch == '=' || ch == ',') {
            continue;
        }
        return true; // has invalid char
    }
    return false;
}

bool CheckTableDescrptor(TableDescriptor* table_desc) {
    if (table_desc->SplitSize() < table_desc->MergeSize() * 5) {
        LOG(ERROR) << "splitsize should be 5 times larger than mergesize"
            << ", splitsize: " << table_desc->SplitSize()
            << ", mergesize: " << table_desc->MergeSize();
        return false;
    }
    return true;
}

/*
 * parses `schema', sets TableDescriptor and notes whether to update lg or cf.
 *
 * an example of schema:
 *   "table:splitsize=100,lg0:storage=disk,lg1:blocksize=5,cf6:ttl=0"
 */
bool ParseSchemaSetTableDescriptor(const string& schema, TableDescriptor* table_desc,
                                   bool* is_update_lg_cf) {
    if (table_desc == NULL) {
        LOG(ERROR) << "parameter `table_desc' is NULL";
        return false;
    }
    std::vector<string> parts;
    string schema_in = RemoveInvisibleChar(schema);
    if (HasInvalidCharInSchema(schema)) {
        LOG(ERROR) << "illegal char(s) in schema: " << schema;
        return false;
    }
    SplitString(schema_in, ",", &parts);
    if (parts.size() == 0) {
        LOG(ERROR) << "illegal schema: " << schema;
        return false;
    }

    for (size_t i = 0; i < parts.size(); i++) {
        string prefix;// "table" | lg name | cf name
        string property; // splitsize/compress/ttl ...
        string value;
        if (!ParsePrefixPropertyValue(parts[i], prefix, property, value)) {
            LOG(ERROR) << "ParsePrefixPropertyValue:illegal schema: " << parts[i];
            return false;
        }
        if (prefix == "" || property == "" || value == "") {
            LOG(ERROR) << "illegal schema: " << parts[i];
            return false;
        }
        Property apair(property, value);
        PropertyList props;
        props.push_back(apair);
        if (prefix == "table") {
            if (property == "rawkey") {
                LOG(ERROR) << "oops, can't reset <rawkey>";
                return false;
            }
            if (!SetTableProperties(props, table_desc)) {
                LOG(ERROR) << "SetTableProperties() failed";
                return false;
            }
        } else if (PrefixType(property) == "lg") {
            *is_update_lg_cf = true;
            LocalityGroupDescriptor* lg_desc =
                const_cast<LocalityGroupDescriptor*>(table_desc->LocalityGroup(prefix));
            if (lg_desc == NULL) {
                LOG(ERROR) << "illegal schema: " << parts[i];
                return false;
            }
            SetLgProperties(props, lg_desc);
        } else if (PrefixType(property) == "cf") {
            *is_update_lg_cf = true;
            ColumnFamilyDescriptor* cf_desc =
                const_cast<ColumnFamilyDescriptor*>(table_desc->ColumnFamily(prefix));
            if (cf_desc == NULL) {
                LOG(ERROR) << "illegal schema: " << parts[i];
                return false;
            }
            SetCfProperties(props, cf_desc);
        } else {
            LOG(ERROR) << "illegal schema: " << parts[i];
            return false;
        }
    }
    if (!CheckTableDescrptor(table_desc)) {
        return false;
    }
    return true;
}

bool FillTableDescriptor(PropTree& schema_tree, TableDescriptor* table_desc) {
    PropTree::Node* table_node = schema_tree.GetRootNode();
    if (schema_tree.MaxDepth() == 1) {
        // kv mode, only have 1 locality group
        // e.g. table1<splitsize=1024, storage=flash>
        table_desc->SetKvOnly();
        LocalityGroupDescriptor* lg_desc;
        lg_desc = table_desc->AddLocalityGroup("kv");
        if (lg_desc == NULL) {
            LOG(ERROR) << "fail to add locality group: " << lg_desc->Name();
            return false;
        }
        for (std::map<string, string>::iterator i = table_node->properties_.begin();
             i != table_node->properties_.end(); ++i) {
            if (!SetTableProperties(i->first, i->second, table_desc) &&
                !SetLgProperties(i->first, i->second, lg_desc)) {
                LOG(ERROR) << "illegal value: " << i->second
                    << " for table property: " << i->first;
                return false;
            }
        }
    } else if (schema_tree.MaxDepth() == 2) {
        // simple table mode, have 1 default lg
        // e.g. table1{cf1, cf2, cf3}
        LocalityGroupDescriptor* lg_desc;
        lg_desc = table_desc->AddLocalityGroup("lg0");
        if (lg_desc == NULL) {
            LOG(ERROR) << "fail to add locality group: " << lg_desc->Name();
            return false;
        }
        // add all column families and properties
        for (size_t i = 0; i < table_node->children_.size(); ++i) {
            PropTree::Node* cf_node = table_node->children_[i];
            ColumnFamilyDescriptor* cf_desc;
            cf_desc = table_desc->AddColumnFamily(cf_node->name_, lg_desc->Name());
            if (cf_desc == NULL) {
                LOG(ERROR) << "fail to add column family: " << cf_desc->Name();
                return false;
            }
            for (std::map<string, string>::iterator it = cf_node->properties_.begin();
                 it != cf_node->properties_.end(); ++it) {
                if (!SetCfProperties(it->first, it->second, cf_desc)) {
                    LOG(ERROR) << "illegal value: " << it->second
                        << " for cf property: " << it->first;
                    return false;
                }
            }
        }
        // set table properties
        for (std::map<string, string>::iterator i = table_node->properties_.begin();
             i != table_node->properties_.end(); ++i) {
            if (!SetTableProperties(i->first, i->second, table_desc)) {
                LOG(ERROR) << "illegal value: " << i->second
                    << " for table property: " << i->first;
                return false;
            }
        }
    } else if (schema_tree.MaxDepth() == 3) {
        // full mode, all elements are user-defined
        // e.g. table1<mergesize=100>{
        //          lg0<storage=memory>{
        //              cf1<maxversions=3>,
        //              cf2<ttl=100>
        //          },
        //          lg1{cf3}
        //      }
        for (size_t i = 0; i < table_node->children_.size(); ++i) {
            PropTree::Node* lg_node = table_node->children_[i];
            LocalityGroupDescriptor* lg_desc;
            lg_desc = table_desc->AddLocalityGroup(lg_node->name_);
            if (lg_desc == NULL) {
                LOG(ERROR) << "fail to add locality group: " << lg_desc->Name();
                return false;
            }
            // add all column families and properties
            for (size_t j = 0; j < lg_node->children_.size(); ++j) {
                PropTree::Node* cf_node = lg_node->children_[j];
                ColumnFamilyDescriptor* cf_desc;
                cf_desc = table_desc->AddColumnFamily(cf_node->name_, lg_desc->Name());
                if (cf_desc == NULL) {
                    LOG(ERROR) << "fail to add column family: " << cf_desc->Name();
                    return false;
                }
                for (std::map<string, string>::iterator it = cf_node->properties_.begin();
                     it != cf_node->properties_.end(); ++it) {
                    if (!SetCfProperties(it->first, it->second, cf_desc)) {
                        LOG(ERROR) << "illegal value: " << it->second
                            << " for cf property: " << it->first;
                        return false;
                    }
                }
            }
            // set locality group properties
            for (std::map<string, string>::iterator it_lg = lg_node->properties_.begin();
                 it_lg != lg_node->properties_.end(); ++it_lg) {
                if (!SetLgProperties(it_lg->first, it_lg->second, lg_desc)) {
                    LOG(ERROR) << "illegal value: " << it_lg->second
                        << " for lg property: " << it_lg->first;
                    return false;
                }
            }
        }
        // set table properties
        for (std::map<string, string>::iterator i = table_node->properties_.begin();
             i != table_node->properties_.end(); ++i) {
            if (!SetTableProperties(i->first, i->second, table_desc)) {
                LOG(ERROR) << "illegal value: " << i->second
                    << " for table property: " << i->first;
                return false;
            }
        }
    } else {
        LOG(FATAL) << "never here.";
    }
    return true;
}

bool ParseSchema(const string& schema, TableDescriptor* table_desc) {
    PropTree schema_tree;
    if (!schema_tree.ParseFromString(schema)) {
        LOG(ERROR) << schema_tree.State();
        LOG(ERROR) << schema;
        return false;
    }

    VLOG(10) << "table to create: " << schema_tree.FormatString();

    if (table_desc->TableName() != "" &&
        table_desc->TableName() != schema_tree.GetRootNode()->name_) {
        LOG(ERROR) << "table name error: " << table_desc->TableName()
            << ":" << schema_tree.GetRootNode()->name_;
        return false;
    }
    table_desc->SetTableName(schema_tree.GetRootNode()->name_);
    if (schema_tree.MaxDepth() != schema_tree.MinDepth() ||
        schema_tree.MaxDepth() == 0 || schema_tree.MaxDepth() > 3) {
        LOG(ERROR) << "schema error: " << schema_tree.FormatString();
        return false;
    }

    if (FillTableDescriptor(schema_tree, table_desc) &&
        CheckTableDescrptor(table_desc)) {
        return true;
    }
    return false;
}

bool ParseScanSchema(const string& schema, ScanDescriptor* desc) {
    std::vector<string> cfs;
    string schema_in;
    string cf, col;
    string::size_type pos;
    if ((pos = schema.find("SELECT ")) != 0) {
        LOG(ERROR) << "illegal scan expression: should be begin with \"SELECT\"";
        return false;
    }
    if ((pos = schema.find(" WHERE ")) != string::npos) {
        schema_in = schema.substr(7, pos - 7);
        string filter_str = schema.substr(pos + 7, schema.size() - pos - 7);
        desc->SetFilterString(filter_str);
    } else {
        schema_in = schema.substr(7);
    }

    schema_in = RemoveInvisibleChar(schema_in);
    if (schema_in == "*") {
        return true;
    }
    SplitString(schema_in, ",", &cfs);
    for (size_t i = 0; i < cfs.size(); ++i) {
        if ((pos = cfs[i].find(":", 0)) == string::npos) {
            // add columnfamily
            desc->AddColumnFamily(cfs[i]);
            VLOG(10) << "add cf: " << cfs[i] << " to scan descriptor";
        } else {
            // add column
            cf = cfs[i].substr(0, pos);
            col = cfs[i].substr(pos + 1);
            desc->AddColumn(cf, col);
            VLOG(10) << "add column: " << cf << ":" << col << " to scan descriptor";
        }
    }
    return true;
}

bool BuildSchema(TableDescriptor* table_desc, string* schema) {
    // build schema string from table descriptor
    if (schema == NULL) {
        LOG(ERROR) << "schema string is NULL.";
        return false;
    }
    if (table_desc == NULL) {
        LOG(ERROR) << "table descriptor is NULL.";
        return false;
    }

    schema->clear();
    int32_t lg_num = table_desc->LocalityGroupNum();
    int32_t cf_num = table_desc->ColumnFamilyNum();
    for (int32_t lg_no = 0; lg_no < lg_num; ++lg_no) {
        const LocalityGroupDescriptor* lg_desc = table_desc->LocalityGroup(lg_no);
        string lg_name = lg_desc->Name();
        if (lg_no > 0) {
            schema->append("|");
        }
        schema->append(lg_name);
        schema->append(":");
        int cf_cnt = 0;
        for (int32_t cf_no = 0; cf_no < cf_num; ++cf_no) {
            const ColumnFamilyDescriptor* cf_desc = table_desc->ColumnFamily(cf_no);
            if (cf_desc->LocalityGroup() == lg_name && cf_desc->Name() != "") {
                if (cf_cnt++ > 0) {
                    schema->append(",");
                }
                schema->append(cf_desc->Name());
            }
        }
    }
    return true;
}
} // namespace tera
