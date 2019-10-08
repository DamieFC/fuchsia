// WARNING: This file is machine generated by fidlc.

#include <lib/fidl/internal.h>

extern "C" {

extern const fidl_type_t fidl_test_coding_Uint8EnumTable;
extern const fidl_type_t fidl_test_coding_Uint8EnumStructTable;
extern const fidl_type_t fidl_test_coding_Uint64EnumTable;
extern const fidl_type_t fidl_test_coding_Uint64EnumStructTable;
extern const fidl_type_t fidl_test_coding_Uint32EnumTable;
extern const fidl_type_t fidl_test_coding_Uint32EnumStructTable;
extern const fidl_type_t fidl_test_coding_Uint16EnumTable;
extern const fidl_type_t fidl_test_coding_Uint16EnumStructTable;
extern const fidl_type_t fidl_test_coding_StructWithManyHandlesTable;
extern const fidl_type_t fidl_test_coding_SmallerTableOfStructWithHandleTable;
extern const fidl_type_t fidl_test_coding_StructWithHandleTable;
extern const fidl_type_t fidl_test_coding_TableOfStructWithHandleTable;
extern const fidl_type_t fidl_test_coding_SampleNullableXUnionStructTable;
extern const fidl_type_t fidl_test_coding_IntStructTable;
extern const fidl_type_t fidl_test_coding_SimpleTableTable;
extern const fidl_type_t fidl_test_coding_SampleXUnionTable;
extern const fidl_type_t fidl_test_coding_SampleXUnionNullableRefTable;
extern const fidl_type_t fidl_test_coding_SampleXUnionStructTable;
extern const fidl_type_t fidl_test_coding_SampleUnionTable;
extern const fidl_type_t fidl_test_coding_SampleStrictXUnionTable;
extern const fidl_type_t fidl_test_coding_SampleStrictXUnionNullableRefTable;
extern const fidl_type_t fidl_test_coding_SampleStrictXUnionStructTable;
extern const fidl_type_t fidl_test_coding_OlderSimpleTableTable;
extern const fidl_type_t fidl_test_coding_NewerSimpleTableTable;
extern const fidl_type_t fidl_test_coding_Int8EnumTable;
extern const fidl_type_t fidl_test_coding_Int8EnumStructTable;
extern const fidl_type_t fidl_test_coding_Int64EnumTable;
extern const fidl_type_t fidl_test_coding_Int64EnumStructTable;
extern const fidl_type_t fidl_test_coding_Int32EnumTable;
extern const fidl_type_t fidl_test_coding_Int32EnumStructTable;
extern const fidl_type_t fidl_test_coding_Int32BitsTable;
extern const fidl_type_t fidl_test_coding_Int32BitsStructTable;
extern const fidl_type_t fidl_test_coding_Int16EnumTable;
extern const fidl_type_t fidl_test_coding_Int16EnumStructTable;
extern const fidl_type_t fidl_test_coding_Int16BitsTable;
extern const fidl_type_t fidl_test_coding_Int16BitsStructTable;


static const fidl_type_t HandlehandlenonnullableTable = fidl_type_t(::fidl::FidlCodedHandle(ZX_OBJ_TYPE_NONE, ::fidl::kNonnullable));

static const fidl_type_t Vector2nonnullable23HandlehandlenonnullableTable = fidl_type_t(::fidl::FidlCodedVector(&HandlehandlenonnullableTable, 2u, 4u, ::fidl::kNonnullable));

static const fidl_type_t Vector4294967295nonnullable6uint32Table = fidl_type_t(::fidl::FidlCodedVector(nullptr, 4294967295u, 4u, ::fidl::kNonnullable));

extern const fidl_type_t fidl_test_coding_LinearizerTestVectorOfUint32RequestTable;
static const ::fidl::FidlStructField Fields52fidl_test_coding_LinearizerTestVectorOfUint32Request[] = {
    ::fidl::FidlStructField(&Vector4294967295nonnullable6uint32Table, 16u, 0u)
};
const fidl_type_t fidl_test_coding_LinearizerTestVectorOfUint32RequestTable = fidl_type_t(::fidl::FidlCodedStruct(Fields52fidl_test_coding_LinearizerTestVectorOfUint32Request, 1u, 32u, "fidl.test.coding/LinearizerTestVectorOfUint32Request"));

static const fidl_type_t String4294967295nonnullableTable = fidl_type_t(::fidl::FidlCodedString(4294967295u, ::fidl::kNonnullable));

static const fidl_type_t Vector4294967295nonnullable27String4294967295nonnullableTable = fidl_type_t(::fidl::FidlCodedVector(&String4294967295nonnullableTable, 4294967295u, 16u, ::fidl::kNonnullable));

extern const fidl_type_t fidl_test_coding_LinearizerTestVectorOfStringRequestTable;
static const ::fidl::FidlStructField Fields52fidl_test_coding_LinearizerTestVectorOfStringRequest[] = {
    ::fidl::FidlStructField(&Vector4294967295nonnullable27String4294967295nonnullableTable, 16u, 0u)
};
const fidl_type_t fidl_test_coding_LinearizerTestVectorOfStringRequestTable = fidl_type_t(::fidl::FidlCodedStruct(Fields52fidl_test_coding_LinearizerTestVectorOfStringRequest, 1u, 32u, "fidl.test.coding/LinearizerTestVectorOfStringRequest"));


static constexpr bool EnumValidatorFor_fidl_test_coding_Uint8Enum(uint64_t v) { return (v == 0ul) || (v == 255ul) || (v == 42ul) || false; }
const fidl_type_t fidl_test_coding_Uint8EnumTable = fidl_type_t(::fidl::FidlCodedEnum(::fidl::FidlCodedPrimitive::kUint8, &EnumValidatorFor_fidl_test_coding_Uint8Enum, "fidl.test.coding/Uint8Enum"));

static const ::fidl::FidlStructField Fields32fidl_test_coding_Uint8EnumStruct[] = {
    ::fidl::FidlStructField(&fidl_test_coding_Uint8EnumTable, 0u, 0u)
};
const fidl_type_t fidl_test_coding_Uint8EnumStructTable = fidl_type_t(::fidl::FidlCodedStruct(Fields32fidl_test_coding_Uint8EnumStruct, 1u, 1u, "fidl.test.coding/Uint8EnumStruct"));

static constexpr bool EnumValidatorFor_fidl_test_coding_Uint64Enum(uint64_t v) { return (v == 0ul) || (v == 18446744073709551615ul) || (v == 42ul) || false; }
const fidl_type_t fidl_test_coding_Uint64EnumTable = fidl_type_t(::fidl::FidlCodedEnum(::fidl::FidlCodedPrimitive::kUint64, &EnumValidatorFor_fidl_test_coding_Uint64Enum, "fidl.test.coding/Uint64Enum"));

static const ::fidl::FidlStructField Fields33fidl_test_coding_Uint64EnumStruct[] = {
    ::fidl::FidlStructField(&fidl_test_coding_Uint64EnumTable, 0u, 0u)
};
const fidl_type_t fidl_test_coding_Uint64EnumStructTable = fidl_type_t(::fidl::FidlCodedStruct(Fields33fidl_test_coding_Uint64EnumStruct, 1u, 8u, "fidl.test.coding/Uint64EnumStruct"));

static constexpr bool EnumValidatorFor_fidl_test_coding_Uint32Enum(uint64_t v) { return (v == 0ul) || (v == 4294967295ul) || (v == 42ul) || false; }
const fidl_type_t fidl_test_coding_Uint32EnumTable = fidl_type_t(::fidl::FidlCodedEnum(::fidl::FidlCodedPrimitive::kUint32, &EnumValidatorFor_fidl_test_coding_Uint32Enum, "fidl.test.coding/Uint32Enum"));

static const ::fidl::FidlStructField Fields33fidl_test_coding_Uint32EnumStruct[] = {
    ::fidl::FidlStructField(&fidl_test_coding_Uint32EnumTable, 0u, 0u)
};
const fidl_type_t fidl_test_coding_Uint32EnumStructTable = fidl_type_t(::fidl::FidlCodedStruct(Fields33fidl_test_coding_Uint32EnumStruct, 1u, 4u, "fidl.test.coding/Uint32EnumStruct"));

static constexpr bool EnumValidatorFor_fidl_test_coding_Uint16Enum(uint64_t v) { return (v == 0ul) || (v == 65535ul) || (v == 42ul) || false; }
const fidl_type_t fidl_test_coding_Uint16EnumTable = fidl_type_t(::fidl::FidlCodedEnum(::fidl::FidlCodedPrimitive::kUint16, &EnumValidatorFor_fidl_test_coding_Uint16Enum, "fidl.test.coding/Uint16Enum"));

static const ::fidl::FidlStructField Fields33fidl_test_coding_Uint16EnumStruct[] = {
    ::fidl::FidlStructField(&fidl_test_coding_Uint16EnumTable, 0u, 0u)
};
const fidl_type_t fidl_test_coding_Uint16EnumStructTable = fidl_type_t(::fidl::FidlCodedStruct(Fields33fidl_test_coding_Uint16EnumStruct, 1u, 2u, "fidl.test.coding/Uint16EnumStruct"));

static const ::fidl::FidlStructField Fields38fidl_test_coding_StructWithManyHandles[] = {
    ::fidl::FidlStructField(&HandlehandlenonnullableTable, 0u, 0u),
    ::fidl::FidlStructField(&HandlehandlenonnullableTable, 4u, 0u),
    ::fidl::FidlStructField(&Vector2nonnullable23HandlehandlenonnullableTable, 8u, 0u)
};
const fidl_type_t fidl_test_coding_StructWithManyHandlesTable = fidl_type_t(::fidl::FidlCodedStruct(Fields38fidl_test_coding_StructWithManyHandles, 3u, 24u, "fidl.test.coding/StructWithManyHandles"));

static const ::fidl::FidlTableField Fields47fidl_test_coding_SmallerTableOfStructWithHandle[] = {
    ::fidl::FidlTableField(&fidl_test_coding_StructWithManyHandlesTable,2u)
};
const fidl_type_t fidl_test_coding_SmallerTableOfStructWithHandleTable = fidl_type_t(::fidl::FidlCodedTable(Fields47fidl_test_coding_SmallerTableOfStructWithHandle, 1u, "fidl.test.coding/SmallerTableOfStructWithHandle"));

static const ::fidl::FidlStructField Fields33fidl_test_coding_StructWithHandle[] = {
    ::fidl::FidlStructField(&HandlehandlenonnullableTable, 0u, 0u)
};
const fidl_type_t fidl_test_coding_StructWithHandleTable = fidl_type_t(::fidl::FidlCodedStruct(Fields33fidl_test_coding_StructWithHandle, 1u, 8u, "fidl.test.coding/StructWithHandle"));

static const ::fidl::FidlTableField Fields40fidl_test_coding_TableOfStructWithHandle[] = {
    ::fidl::FidlTableField(&fidl_test_coding_StructWithHandleTable,1u),
    ::fidl::FidlTableField(&fidl_test_coding_StructWithManyHandlesTable,2u)
};
const fidl_type_t fidl_test_coding_TableOfStructWithHandleTable = fidl_type_t(::fidl::FidlCodedTable(Fields40fidl_test_coding_TableOfStructWithHandle, 2u, "fidl.test.coding/TableOfStructWithHandle"));

static const ::fidl::FidlStructField Fields43fidl_test_coding_SampleNullableXUnionStruct[] = {
    ::fidl::FidlStructField(&fidl_test_coding_SampleXUnionNullableRefTable, 0u, 0u)
};
const fidl_type_t fidl_test_coding_SampleNullableXUnionStructTable = fidl_type_t(::fidl::FidlCodedStruct(Fields43fidl_test_coding_SampleNullableXUnionStruct, 1u, 24u, "fidl.test.coding/SampleNullableXUnionStruct"));

static const ::fidl::FidlStructField Fields26fidl_test_coding_IntStruct[] = {};
const fidl_type_t fidl_test_coding_IntStructTable = fidl_type_t(::fidl::FidlCodedStruct(Fields26fidl_test_coding_IntStruct, 0u, 8u, "fidl.test.coding/IntStruct"));

static const ::fidl::FidlTableField Fields28fidl_test_coding_SimpleTable[] = {
    ::fidl::FidlTableField(&fidl_test_coding_IntStructTable,1u),
    ::fidl::FidlTableField(&fidl_test_coding_IntStructTable,5u)
};
const fidl_type_t fidl_test_coding_SimpleTableTable = fidl_type_t(::fidl::FidlCodedTable(Fields28fidl_test_coding_SimpleTable, 2u, "fidl.test.coding/SimpleTable"));

static const ::fidl::FidlXUnionField Fields29fidl_test_coding_SampleXUnion[] = {
    ::fidl::FidlXUnionField(&::fidl::internal::kInt32Table,319709411u),
    ::fidl::FidlXUnionField(&fidl_test_coding_IntStructTable,376675050u),
    ::fidl::FidlXUnionField(&fidl_test_coding_SimpleTableTable,586453270u)
};
const fidl_type_t fidl_test_coding_SampleXUnionTable = fidl_type_t(::fidl::FidlCodedXUnion(3u, Fields29fidl_test_coding_SampleXUnion, ::fidl::kNonnullable, "fidl.test.coding/SampleXUnion", ::fidl::kFlexible));

static const ::fidl::FidlXUnionField Fields40fidl_test_coding_SampleXUnionNullableRef[] = {
    ::fidl::FidlXUnionField(&::fidl::internal::kInt32Table,319709411u),
    ::fidl::FidlXUnionField(&fidl_test_coding_IntStructTable,376675050u),
    ::fidl::FidlXUnionField(&fidl_test_coding_SimpleTableTable,586453270u)
};
const fidl_type_t fidl_test_coding_SampleXUnionNullableRefTable = fidl_type_t(::fidl::FidlCodedXUnion(3u, Fields40fidl_test_coding_SampleXUnionNullableRef, ::fidl::kNullable, "fidl.test.coding/SampleXUnion", ::fidl::kFlexible));

static const ::fidl::FidlStructField Fields35fidl_test_coding_SampleXUnionStruct[] = {
    ::fidl::FidlStructField(&fidl_test_coding_SampleXUnionTable, 0u, 0u)
};
const fidl_type_t fidl_test_coding_SampleXUnionStructTable = fidl_type_t(::fidl::FidlCodedStruct(Fields35fidl_test_coding_SampleXUnionStruct, 1u, 24u, "fidl.test.coding/SampleXUnionStruct"));

static const ::fidl::FidlUnionField Fields28fidl_test_coding_SampleUnion[] = {
    ::fidl::FidlUnionField(&fidl_test_coding_IntStructTable, 8u),
    ::fidl::FidlUnionField(&fidl_test_coding_SimpleTableTable, 0u),
    ::fidl::FidlUnionField(nullptr, 12u)
};
const fidl_type_t fidl_test_coding_SampleUnionTable = fidl_type_t(::fidl::FidlCodedUnion(Fields28fidl_test_coding_SampleUnion, 3u, 8u, 24u, "fidl.test.coding/SampleUnion"));

static const ::fidl::FidlXUnionField Fields35fidl_test_coding_SampleStrictXUnion[] = {
    ::fidl::FidlXUnionField(&::fidl::internal::kInt32Table,419938224u),
    ::fidl::FidlXUnionField(&fidl_test_coding_SimpleTableTable,915108668u),
    ::fidl::FidlXUnionField(&fidl_test_coding_IntStructTable,1928460319u)
};
const fidl_type_t fidl_test_coding_SampleStrictXUnionTable = fidl_type_t(::fidl::FidlCodedXUnion(3u, Fields35fidl_test_coding_SampleStrictXUnion, ::fidl::kNonnullable, "fidl.test.coding/SampleStrictXUnion", ::fidl::kStrict));

static const ::fidl::FidlXUnionField Fields46fidl_test_coding_SampleStrictXUnionNullableRef[] = {
    ::fidl::FidlXUnionField(&::fidl::internal::kInt32Table,419938224u),
    ::fidl::FidlXUnionField(&fidl_test_coding_SimpleTableTable,915108668u),
    ::fidl::FidlXUnionField(&fidl_test_coding_IntStructTable,1928460319u)
};
const fidl_type_t fidl_test_coding_SampleStrictXUnionNullableRefTable = fidl_type_t(::fidl::FidlCodedXUnion(3u, Fields46fidl_test_coding_SampleStrictXUnionNullableRef, ::fidl::kNullable, "fidl.test.coding/SampleStrictXUnion", ::fidl::kStrict));

static const ::fidl::FidlStructField Fields41fidl_test_coding_SampleStrictXUnionStruct[] = {
    ::fidl::FidlStructField(&fidl_test_coding_SampleStrictXUnionTable, 0u, 0u)
};
const fidl_type_t fidl_test_coding_SampleStrictXUnionStructTable = fidl_type_t(::fidl::FidlCodedStruct(Fields41fidl_test_coding_SampleStrictXUnionStruct, 1u, 24u, "fidl.test.coding/SampleStrictXUnionStruct"));

static const ::fidl::FidlTableField Fields33fidl_test_coding_OlderSimpleTable[] = {
    ::fidl::FidlTableField(&fidl_test_coding_IntStructTable,1u)
};
const fidl_type_t fidl_test_coding_OlderSimpleTableTable = fidl_type_t(::fidl::FidlCodedTable(Fields33fidl_test_coding_OlderSimpleTable, 1u, "fidl.test.coding/OlderSimpleTable"));

static const ::fidl::FidlTableField Fields33fidl_test_coding_NewerSimpleTable[] = {
    ::fidl::FidlTableField(&fidl_test_coding_IntStructTable,1u),
    ::fidl::FidlTableField(&fidl_test_coding_IntStructTable,5u),
    ::fidl::FidlTableField(&fidl_test_coding_IntStructTable,6u)
};
const fidl_type_t fidl_test_coding_NewerSimpleTableTable = fidl_type_t(::fidl::FidlCodedTable(Fields33fidl_test_coding_NewerSimpleTable, 3u, "fidl.test.coding/NewerSimpleTable"));

static constexpr bool EnumValidatorFor_fidl_test_coding_Int8Enum(uint64_t v) { return (v == 18446744073709551488ul) || (v == 127ul) || (v == 42ul) || false; }
const fidl_type_t fidl_test_coding_Int8EnumTable = fidl_type_t(::fidl::FidlCodedEnum(::fidl::FidlCodedPrimitive::kInt8, &EnumValidatorFor_fidl_test_coding_Int8Enum, "fidl.test.coding/Int8Enum"));

static const ::fidl::FidlStructField Fields31fidl_test_coding_Int8EnumStruct[] = {
    ::fidl::FidlStructField(&fidl_test_coding_Int8EnumTable, 0u, 0u)
};
const fidl_type_t fidl_test_coding_Int8EnumStructTable = fidl_type_t(::fidl::FidlCodedStruct(Fields31fidl_test_coding_Int8EnumStruct, 1u, 1u, "fidl.test.coding/Int8EnumStruct"));

static constexpr bool EnumValidatorFor_fidl_test_coding_Int64Enum(uint64_t v) { return (v == 9223372036854775808ul) || (v == 9223372036854775807ul) || (v == 42ul) || false; }
const fidl_type_t fidl_test_coding_Int64EnumTable = fidl_type_t(::fidl::FidlCodedEnum(::fidl::FidlCodedPrimitive::kInt64, &EnumValidatorFor_fidl_test_coding_Int64Enum, "fidl.test.coding/Int64Enum"));

static const ::fidl::FidlStructField Fields32fidl_test_coding_Int64EnumStruct[] = {
    ::fidl::FidlStructField(&fidl_test_coding_Int64EnumTable, 0u, 0u)
};
const fidl_type_t fidl_test_coding_Int64EnumStructTable = fidl_type_t(::fidl::FidlCodedStruct(Fields32fidl_test_coding_Int64EnumStruct, 1u, 8u, "fidl.test.coding/Int64EnumStruct"));

static constexpr bool EnumValidatorFor_fidl_test_coding_Int32Enum(uint64_t v) { return (v == 18446744071562067968ul) || (v == 2147483647ul) || (v == 42ul) || false; }
const fidl_type_t fidl_test_coding_Int32EnumTable = fidl_type_t(::fidl::FidlCodedEnum(::fidl::FidlCodedPrimitive::kInt32, &EnumValidatorFor_fidl_test_coding_Int32Enum, "fidl.test.coding/Int32Enum"));

static const ::fidl::FidlStructField Fields32fidl_test_coding_Int32EnumStruct[] = {
    ::fidl::FidlStructField(&fidl_test_coding_Int32EnumTable, 0u, 0u)
};
const fidl_type_t fidl_test_coding_Int32EnumStructTable = fidl_type_t(::fidl::FidlCodedStruct(Fields32fidl_test_coding_Int32EnumStruct, 1u, 4u, "fidl.test.coding/Int32EnumStruct"));

const fidl_type_t fidl_test_coding_Int32BitsTable = fidl_type_t(::fidl::FidlCodedBits(::fidl::FidlCodedPrimitive::kUint32, 67110976ul, "fidl.test.coding/Int32Bits"));

static const ::fidl::FidlStructField Fields32fidl_test_coding_Int32BitsStruct[] = {
    ::fidl::FidlStructField(&fidl_test_coding_Int32BitsTable, 0u, 0u)
};
const fidl_type_t fidl_test_coding_Int32BitsStructTable = fidl_type_t(::fidl::FidlCodedStruct(Fields32fidl_test_coding_Int32BitsStruct, 1u, 4u, "fidl.test.coding/Int32BitsStruct"));

static constexpr bool EnumValidatorFor_fidl_test_coding_Int16Enum(uint64_t v) { return (v == 18446744073709518848ul) || (v == 32767ul) || (v == 42ul) || false; }
const fidl_type_t fidl_test_coding_Int16EnumTable = fidl_type_t(::fidl::FidlCodedEnum(::fidl::FidlCodedPrimitive::kInt16, &EnumValidatorFor_fidl_test_coding_Int16Enum, "fidl.test.coding/Int16Enum"));

static const ::fidl::FidlStructField Fields32fidl_test_coding_Int16EnumStruct[] = {
    ::fidl::FidlStructField(&fidl_test_coding_Int16EnumTable, 0u, 0u)
};
const fidl_type_t fidl_test_coding_Int16EnumStructTable = fidl_type_t(::fidl::FidlCodedStruct(Fields32fidl_test_coding_Int16EnumStruct, 1u, 2u, "fidl.test.coding/Int16EnumStruct"));

const fidl_type_t fidl_test_coding_Int16BitsTable = fidl_type_t(::fidl::FidlCodedBits(::fidl::FidlCodedPrimitive::kUint16, 21ul, "fidl.test.coding/Int16Bits"));

static const ::fidl::FidlStructField Fields32fidl_test_coding_Int16BitsStruct[] = {
    ::fidl::FidlStructField(&fidl_test_coding_Int16BitsTable, 0u, 0u)
};
const fidl_type_t fidl_test_coding_Int16BitsStructTable = fidl_type_t(::fidl::FidlCodedStruct(Fields32fidl_test_coding_Int16BitsStruct, 1u, 2u, "fidl.test.coding/Int16BitsStruct"));

} // extern "C"
