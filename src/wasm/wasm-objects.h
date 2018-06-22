// Copyright 2016 the V8 project authors. All rights reserved.  Use of
// this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_OBJECTS_H_
#define V8_WASM_WASM_OBJECTS_H_

#include "src/base/bits.h"
#include "src/debug/debug.h"
#include "src/debug/interface-types.h"
#include "src/heap/heap.h"
#include "src/objects.h"
#include "src/objects/script.h"
#include "src/signature.h"
#include "src/wasm/value-type.h"

// Has to be the last include (doesn't have include guards)
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {
namespace wasm {
class InterpretedFrame;
struct InterpretedFrameDeleter;
class NativeModule;
struct ModuleEnv;
class WasmCode;
struct WasmModule;
class SignatureMap;
class WireBytesRef;
class WasmInterpreter;
using FunctionSig = Signature<ValueType>;
}  // namespace wasm

class BreakPoint;
class JSArrayBuffer;
class FixedArrayOfWeakCells;
class SeqOneByteString;
class WasmCompiledModule;
class WasmDebugInfo;
class WasmInstanceObject;

template <class CppType>
class Managed;

#define DECL_OPTIONAL_ACCESSORS(name, type) \
  V8_INLINE bool has_##name();              \
  DECL_ACCESSORS(name, type)

// An entry in an indirect function table (IFT).
// Each entry in the IFT has the following fields:
// - instance = target instance
// - sig_id   = signature id of function
// - target   = entrypoint to wasm code for the function, or wasm-to-js wrapper
class IndirectFunctionTableEntry {
 public:
  inline IndirectFunctionTableEntry(Handle<WasmInstanceObject>, int index);

  void clear();
  void set(int sig_id, WasmInstanceObject* instance, Address call_target);

  WasmInstanceObject* instance();
  int sig_id();
  Address target();

 private:
  Handle<WasmInstanceObject> const instance_;
  int const index_;
};

// An entry for an imported function.
// (note this is not called a "table" since it is not dynamically indexed).
// The imported function entries are used to call imported functions.
// For each imported function there is an entry which is either:
//   - an imported JSReceiver, which has fields
//      - instance = importing instance
//      - receiver = JSReceiver, either a JS function or other callable
//      - target   = pointer to wasm-to-js wrapper code entrypoint
//   - an imported wasm function from another instance, which has fields
//      - instance = target instance
//      - target   = entrypoint for the function
class ImportedFunctionEntry {
 public:
  inline ImportedFunctionEntry(Handle<WasmInstanceObject>, int index);

  // Initialize this entry as a {JSReceiver} call.
  void set_wasm_to_js(JSReceiver* callable,
                      const wasm::WasmCode* wasm_to_js_wrapper);
  // Initialize this entry as a WASM to WASM call.
  void set_wasm_to_wasm(WasmInstanceObject* target_instance,
                        Address call_target);

  WasmInstanceObject* instance();
  JSReceiver* callable();
  Address target();
  bool is_js_receiver_entry();

 private:
  Handle<WasmInstanceObject> const instance_;
  int const index_;
};

// Representation of a WebAssembly.Module JavaScript-level object.
class WasmModuleObject : public JSObject {
 public:
  DECL_CAST(WasmModuleObject)

  DECL_ACCESSORS(managed_native_module, Managed<wasm::NativeModule>)
  inline wasm::NativeModule* native_module();
  DECL_ACCESSORS(compiled_module, WasmCompiledModule)
  DECL_ACCESSORS(export_wrappers, FixedArray)
  DECL_ACCESSORS(managed_module, Managed<wasm::WasmModule>)
  inline wasm::WasmModule* module() const;
  DECL_ACCESSORS(script, Script)
  DECL_OPTIONAL_ACCESSORS(asm_js_offset_table, ByteArray)
  DECL_OPTIONAL_ACCESSORS(breakpoint_infos, FixedArray)
  inline void reset_breakpoint_infos();

  // Dispatched behavior.
  DECL_PRINTER(WasmModuleObject)
  DECL_VERIFIER(WasmModuleObject)

// Layout description.
#define WASM_MODULE_OBJECT_FIELDS(V)       \
  V(kNativeModuleOffset, kPointerSize)     \
  V(kCompiledModuleOffset, kPointerSize)   \
  V(kExportWrappersOffset, kPointerSize)   \
  V(kManagedModuleOffset, kPointerSize)    \
  V(kScriptOffset, kPointerSize)           \
  V(kAsmJsOffsetTableOffset, kPointerSize) \
  V(kBreakPointInfosOffset, kPointerSize)  \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize,
                                WASM_MODULE_OBJECT_FIELDS)
#undef WASM_MODULE_OBJECT_FIELDS

  static Handle<WasmModuleObject> New(
      Isolate* isolate, Handle<FixedArray> export_wrappers,
      std::shared_ptr<wasm::WasmModule> module, wasm::ModuleEnv& env,
      std::unique_ptr<const uint8_t[]> wire_bytes, size_t wire_bytes_len,
      Handle<Script> script, Handle<ByteArray> asm_js_offset_table);

  // Set a breakpoint on the given byte position inside the given module.
  // This will affect all live and future instances of the module.
  // The passed position might be modified to point to the next breakable
  // location inside the same function.
  // If it points outside a function, or behind the last breakable location,
  // this function returns false and does not set any breakpoint.
  static bool SetBreakPoint(Handle<WasmModuleObject>, int* position,
                            Handle<BreakPoint> break_point);

  static void ValidateStateForTesting(Isolate* isolate,
                                      Handle<WasmModuleObject> module);

  // Check whether this module was generated from asm.js source.
  inline bool is_asm_js();

  static void AddBreakpoint(Handle<WasmModuleObject>, int position,
                            Handle<BreakPoint> break_point);

  static void SetBreakpointsOnNewInstance(Handle<WasmModuleObject>,
                                          Handle<WasmInstanceObject>);

  // Get the module name, if set. Returns an empty handle otherwise.
  static MaybeHandle<String> GetModuleNameOrNull(Isolate*,
                                                 Handle<WasmModuleObject>);

  // Get the function name of the function identified by the given index.
  // Returns a null handle if the function is unnamed or the name is not a valid
  // UTF-8 string.
  static MaybeHandle<String> GetFunctionNameOrNull(Isolate*,
                                                   Handle<WasmModuleObject>,
                                                   uint32_t func_index);

  // Get the function name of the function identified by the given index.
  // Returns "<WASM UNNAMED>" if the function is unnamed or the name is not a
  // valid UTF-8 string.
  static Handle<String> GetFunctionName(Isolate*, Handle<WasmModuleObject>,
                                        uint32_t func_index);

  // Get the raw bytes of the function name of the function identified by the
  // given index.
  // Meant to be used for debugging or frame printing.
  // Does not allocate, hence gc-safe.
  Vector<const uint8_t> GetRawFunctionName(uint32_t func_index);

  // Return the byte offset of the function identified by the given index.
  // The offset will be relative to the start of the module bytes.
  // Returns -1 if the function index is invalid.
  int GetFunctionOffset(uint32_t func_index);

  // Returns the function containing the given byte offset.
  // Returns -1 if the byte offset is not contained in any function of this
  // module.
  int GetContainingFunction(uint32_t byte_offset);

  // Translate from byte offset in the module to function number and byte offset
  // within that function, encoded as line and column in the position info.
  // Returns true if the position is valid inside this module, false otherwise.
  bool GetPositionInfo(uint32_t position, Script::PositionInfo* info);

  // Get the source position from a given function index and byte offset,
  // for either asm.js or pure WASM modules.
  static int GetSourcePosition(Handle<WasmModuleObject>, uint32_t func_index,
                               uint32_t byte_offset,
                               bool is_at_number_conversion);

  // Compute the disassembly of a wasm function.
  // Returns the disassembly string and a list of <byte_offset, line, column>
  // entries, mapping wasm byte offsets to line and column in the disassembly.
  // The list is guaranteed to be ordered by the byte_offset.
  // Returns an empty string and empty vector if the function index is invalid.
  debug::WasmDisassembly DisassembleFunction(int func_index);

  // Extract a portion of the wire bytes as UTF-8 string.
  // Returns a null handle if the respective bytes do not form a valid UTF-8
  // string.
  static MaybeHandle<String> ExtractUtf8StringFromModuleBytes(
      Isolate* isolate, Handle<WasmModuleObject>, wasm::WireBytesRef ref);
  static MaybeHandle<String> ExtractUtf8StringFromModuleBytes(
      Isolate* isolate, Vector<const uint8_t> wire_byte,
      wasm::WireBytesRef ref);

  // Get a list of all possible breakpoints within a given range of this module.
  bool GetPossibleBreakpoints(const debug::Location& start,
                              const debug::Location& end,
                              std::vector<debug::BreakLocation>* locations);

  // Return an empty handle if no breakpoint is hit at that location, or a
  // FixedArray with all hit breakpoint objects.
  static MaybeHandle<FixedArray> CheckBreakPoints(Isolate*,
                                                  Handle<WasmModuleObject>,
                                                  int position);
};

// Representation of a WebAssembly.Table JavaScript-level object.
class WasmTableObject : public JSObject {
 public:
  DECL_CAST(WasmTableObject)

  DECL_ACCESSORS(functions, FixedArray)
  // TODO(titzer): introduce DECL_I64_ACCESSORS macro
  DECL_ACCESSORS(maximum_length, Object)
  DECL_ACCESSORS(dispatch_tables, FixedArray)

// Layout description.
#define WASM_TABLE_OBJECT_FIELDS(V)      \
  V(kFunctionsOffset, kPointerSize)      \
  V(kMaximumLengthOffset, kPointerSize)  \
  V(kDispatchTablesOffset, kPointerSize) \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize, WASM_TABLE_OBJECT_FIELDS)
#undef WASM_TABLE_OBJECT_FIELDS

  inline uint32_t current_length();
  void Grow(Isolate* isolate, uint32_t count);

  static Handle<WasmTableObject> New(Isolate* isolate, uint32_t initial,
                                     int64_t maximum,
                                     Handle<FixedArray>* js_functions);
  static void AddDispatchTable(Isolate* isolate, Handle<WasmTableObject> table,
                               Handle<WasmInstanceObject> instance,
                               int table_index);

  static void Set(Isolate* isolate, Handle<WasmTableObject> table,
                  int32_t index, Handle<JSFunction> function);

  static void UpdateDispatchTables(Isolate* isolate,
                                   Handle<WasmTableObject> table,
                                   int table_index, wasm::FunctionSig* sig,
                                   Handle<WasmInstanceObject> from_instance,
                                   Address call_target);

  static void ClearDispatchTables(Isolate* isolate,
                                  Handle<WasmTableObject> table, int index);
};

// Representation of a WebAssembly.Memory JavaScript-level object.
class WasmMemoryObject : public JSObject {
 public:
  DECL_CAST(WasmMemoryObject)

  DECL_ACCESSORS(array_buffer, JSArrayBuffer)
  DECL_INT_ACCESSORS(maximum_pages)
  DECL_OPTIONAL_ACCESSORS(instances, FixedArrayOfWeakCells)

// Layout description.
#define WASM_MEMORY_OBJECT_FIELDS(V)   \
  V(kArrayBufferOffset, kPointerSize)  \
  V(kMaximumPagesOffset, kPointerSize) \
  V(kInstancesOffset, kPointerSize)    \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize,
                                WASM_MEMORY_OBJECT_FIELDS)
#undef WASM_MEMORY_OBJECT_FIELDS

  // Add an instance to the internal (weak) list.
  static void AddInstance(Isolate* isolate, Handle<WasmMemoryObject> memory,
                          Handle<WasmInstanceObject> object);
  // Remove an instance from the internal (weak) list.
  static void RemoveInstance(Isolate* isolate, Handle<WasmMemoryObject> memory,
                             Handle<WasmInstanceObject> object);
  uint32_t current_pages();
  inline bool has_maximum_pages();

  // Return whether the underlying backing store has guard regions large enough
  // to be used with trap handlers.
  bool has_full_guard_region(Isolate* isolate);

  V8_EXPORT_PRIVATE static Handle<WasmMemoryObject> New(
      Isolate* isolate, MaybeHandle<JSArrayBuffer> buffer, int32_t maximum);

  static int32_t Grow(Isolate*, Handle<WasmMemoryObject>, uint32_t pages);
};

// Representation of a WebAssembly.Global JavaScript-level object.
class WasmGlobalObject : public JSObject {
 public:
  DECL_CAST(WasmGlobalObject)

  DECL_ACCESSORS(array_buffer, JSArrayBuffer)
  DECL_INT32_ACCESSORS(offset)
  DECL_INT_ACCESSORS(flags)
  DECL_PRIMITIVE_ACCESSORS(type, wasm::ValueType)
  DECL_BOOLEAN_ACCESSORS(is_mutable)

#define WASM_GLOBAL_OBJECT_FLAGS_BIT_FIELDS(V, _) \
  V(TypeBits, wasm::ValueType, 8, _)              \
  V(IsMutableBit, bool, 1, _)

  DEFINE_BIT_FIELDS(WASM_GLOBAL_OBJECT_FLAGS_BIT_FIELDS)

#undef WASM_GLOBAL_OBJECT_FLAGS_BIT_FIELDS

// Layout description.
#define WASM_GLOBAL_OBJECT_FIELDS(V)  \
  V(kArrayBufferOffset, kPointerSize) \
  V(kOffsetOffset, kPointerSize)      \
  V(kFlagsOffset, kPointerSize)       \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize,
                                WASM_GLOBAL_OBJECT_FIELDS)
#undef WASM_GLOBAL_OBJECT_FIELDS

  V8_EXPORT_PRIVATE static MaybeHandle<WasmGlobalObject> New(
      Isolate* isolate, MaybeHandle<JSArrayBuffer> buffer, wasm::ValueType type,
      int32_t offset, bool is_mutable);

  inline int type_size() const;

  inline int32_t GetI32();
  inline int64_t GetI64();
  inline float GetF32();
  inline double GetF64();

  inline void SetI32(int32_t value);
  inline void SetI64(int64_t value);
  inline void SetF32(float value);
  inline void SetF64(double value);

 private:
  // This function returns the address of the global's data in the
  // JSArrayBuffer. This buffer may be allocated on-heap, in which case it may
  // not have a fixed address.
  inline Address address() const;
};

// Representation of a WebAssembly.Instance JavaScript-level object.
class WasmInstanceObject : public JSObject {
 public:
  DECL_CAST(WasmInstanceObject)

  DECL_ACCESSORS(compiled_module, WasmCompiledModule)
  DECL_ACCESSORS(module_object, WasmModuleObject)
  DECL_ACCESSORS(exports_object, JSObject)
  DECL_ACCESSORS(native_context, Context)
  DECL_OPTIONAL_ACCESSORS(memory_object, WasmMemoryObject)
  DECL_OPTIONAL_ACCESSORS(globals_buffer, JSArrayBuffer)
  DECL_OPTIONAL_ACCESSORS(imported_mutable_globals_buffers, FixedArray)
  DECL_OPTIONAL_ACCESSORS(debug_info, WasmDebugInfo)
  DECL_OPTIONAL_ACCESSORS(table_object, WasmTableObject)
  DECL_ACCESSORS(imported_function_instances, FixedArray)
  DECL_ACCESSORS(imported_function_callables, FixedArray)
  DECL_OPTIONAL_ACCESSORS(indirect_function_table_instances, FixedArray)
  DECL_OPTIONAL_ACCESSORS(managed_native_allocations, Foreign)
  DECL_ACCESSORS(undefined_value, Oddball)
  DECL_ACCESSORS(null_value, Oddball)
  DECL_ACCESSORS(centry_stub, Code)
  DECL_PRIMITIVE_ACCESSORS(memory_start, byte*)
  DECL_PRIMITIVE_ACCESSORS(memory_size, uint32_t)
  DECL_PRIMITIVE_ACCESSORS(memory_mask, uint32_t)
  DECL_PRIMITIVE_ACCESSORS(roots_array_address, Address)
  DECL_PRIMITIVE_ACCESSORS(stack_limit_address, Address)
  DECL_PRIMITIVE_ACCESSORS(imported_function_targets, Address*)
  DECL_PRIMITIVE_ACCESSORS(globals_start, byte*)
  DECL_PRIMITIVE_ACCESSORS(imported_mutable_globals, Address*)
  DECL_PRIMITIVE_ACCESSORS(indirect_function_table_size, uint32_t)
  DECL_PRIMITIVE_ACCESSORS(indirect_function_table_sig_ids, uint32_t*)
  DECL_PRIMITIVE_ACCESSORS(indirect_function_table_targets, Address*)

  // Dispatched behavior.
  DECL_PRINTER(WasmInstanceObject)
  DECL_VERIFIER(WasmInstanceObject)

// Layout description.
#define WASM_INSTANCE_OBJECT_FIELDS(V)                                  \
  V(kCompiledModuleOffset, kPointerSize)                                \
  V(kModuleObjectOffset, kPointerSize)                                  \
  V(kExportsObjectOffset, kPointerSize)                                 \
  V(kNativeContextOffset, kPointerSize)                                 \
  V(kMemoryObjectOffset, kPointerSize)                                  \
  V(kGlobalsBufferOffset, kPointerSize)                                 \
  V(kImportedMutableGlobalsBuffersOffset, kPointerSize)                 \
  V(kDebugInfoOffset, kPointerSize)                                     \
  V(kTableObjectOffset, kPointerSize)                                   \
  V(kImportedFunctionInstancesOffset, kPointerSize)                     \
  V(kImportedFunctionCallablesOffset, kPointerSize)                     \
  V(kIndirectFunctionTableInstancesOffset, kPointerSize)                \
  V(kManagedNativeAllocationsOffset, kPointerSize)                      \
  V(kUndefinedValueOffset, kPointerSize)                                \
  V(kNullValueOffset, kPointerSize)                                     \
  V(kCEntryStubOffset, kPointerSize)                                    \
  V(kFirstUntaggedOffset, 0)                             /* marker */   \
  V(kMemoryStartOffset, kPointerSize)                    /* untagged */ \
  V(kMemorySizeOffset, kUInt32Size)                      /* untagged */ \
  V(kMemoryMaskOffset, kUInt32Size)                      /* untagged */ \
  V(kRootsArrayAddressOffset, kPointerSize)              /* untagged */ \
  V(kStackLimitAddressOffset, kPointerSize)              /* untagged */ \
  V(kImportedFunctionTargetsOffset, kPointerSize)        /* untagged */ \
  V(kGlobalsStartOffset, kPointerSize)                   /* untagged */ \
  V(kImportedMutableGlobalsOffset, kPointerSize)         /* untagged */ \
  V(kIndirectFunctionTableSigIdsOffset, kPointerSize)    /* untagged */ \
  V(kIndirectFunctionTableTargetsOffset, kPointerSize)   /* untagged */ \
  V(kIndirectFunctionTableSizeOffset, kUInt32Size)       /* untagged */ \
  V(k64BitArchPaddingOffset, kPointerSize - kUInt32Size) /* padding */  \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize,
                                WASM_INSTANCE_OBJECT_FIELDS)
#undef WASM_INSTANCE_OBJECT_FIELDS

  V8_EXPORT_PRIVATE wasm::WasmModule* module();

  static bool EnsureIndirectFunctionTableWithMinimumSize(
      Handle<WasmInstanceObject> instance, uint32_t minimum_size);

  bool has_indirect_function_table();

  void SetRawMemory(byte* mem_start, uint32_t mem_size);

  // Get the debug info associated with the given wasm object.
  // If no debug info exists yet, it is created automatically.
  static Handle<WasmDebugInfo> GetOrCreateDebugInfo(Handle<WasmInstanceObject>);

  static Handle<WasmInstanceObject> New(Isolate*, Handle<WasmModuleObject>,
                                        Handle<WasmCompiledModule>);

  static void ValidateInstancesChainForTesting(
      Isolate* isolate, Handle<WasmModuleObject> module_obj,
      int instance_count);

  static void InstallFinalizer(Isolate* isolate,
                               Handle<WasmInstanceObject> instance);

  Address GetCallTarget(uint32_t func_index);

  // Iterates all fields in the object except the untagged fields.
  class BodyDescriptor;
  // No weak fields.
  typedef BodyDescriptor BodyDescriptorWeak;
};

// A WASM function that is wrapped and exported to JavaScript.
class WasmExportedFunction : public JSFunction {
 public:
  WasmInstanceObject* instance();
  V8_EXPORT_PRIVATE int function_index();

  V8_EXPORT_PRIVATE static WasmExportedFunction* cast(Object* object);
  static bool IsWasmExportedFunction(Object* object);

  static Handle<WasmExportedFunction> New(Isolate* isolate,
                                          Handle<WasmInstanceObject> instance,
                                          MaybeHandle<String> maybe_name,
                                          int func_index, int arity,
                                          Handle<Code> export_wrapper);

  Address GetWasmCallTarget();
};

// Information for a WasmExportedFunction which is referenced as the function
// data of the SharedFunctionInfo underlying the function. For details please
// see the {SharedFunctionInfo::HasWasmExportedFunctionData} predicate.
class WasmExportedFunctionData : public Struct {
 public:
  DECL_ACCESSORS(wrapper_code, Code);
  DECL_ACCESSORS(instance, WasmInstanceObject)
  DECL_INT_ACCESSORS(function_index);

  DECL_CAST(WasmExportedFunctionData)

  // Dispatched behavior.
  DECL_PRINTER(WasmExportedFunctionData)
  DECL_VERIFIER(WasmExportedFunctionData)

// Layout description.
#define WASM_EXPORTED_FUNCTION_DATA_FIELDS(V) \
  V(kWrapperCodeOffset, kPointerSize)         \
  V(kInstanceOffset, kPointerSize)            \
  V(kFunctionIndexOffset, kPointerSize)       \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize,
                                WASM_EXPORTED_FUNCTION_DATA_FIELDS)
#undef WASM_EXPORTED_FUNCTION_DATA_FIELDS
};

// This represents the set of wasm compiled functions, together
// with all the information necessary for re-specializing them.
class WasmCompiledModule : public Struct {
 public:
  DECL_CAST(WasmCompiledModule)

  // Dispatched behavior.
  DECL_PRINTER(WasmCompiledModule)
  DECL_VERIFIER(WasmCompiledModule)

// Layout description.
#define WASM_COMPILED_MODULE_FIELDS(V)          \
  V(kNextInstanceOffset, kPointerSize)          \
  V(kPrevInstanceOffset, kPointerSize)          \
  V(kOwningInstanceOffset, kPointerSize)        \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize,
                                WASM_COMPILED_MODULE_FIELDS)
#undef WASM_COMPILED_MODULE_FIELDS

#define WCM_OBJECT_OR_WEAK(TYPE, NAME, SETTER_MODIFIER) \
 public:                                                \
  inline TYPE* NAME() const;                            \
  inline bool has_##NAME() const;                       \
  inline void reset_##NAME();                           \
                                                        \
  SETTER_MODIFIER:                                      \
  inline void set_##NAME(TYPE* value,                   \
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

#define WCM_OBJECT(TYPE, NAME) WCM_OBJECT_OR_WEAK(TYPE, NAME, public)

#define WCM_CONST_OBJECT(TYPE, NAME) WCM_OBJECT_OR_WEAK(TYPE, NAME, private)

#define WCM_WEAK_LINK(TYPE, NAME)                   \
  WCM_OBJECT_OR_WEAK(WeakCell, weak_##NAME, public) \
                                                    \
 public:                                            \
  inline TYPE* NAME() const;

  // Add values here if they are required for creating new instances or
  // for deserialization, and if they are serializable.
  // By default, instance values go to WasmInstanceObject, however, if
  // we embed the generated code with a value, then we track that value here.
  WCM_CONST_OBJECT(WasmCompiledModule, next_instance)
  WCM_CONST_OBJECT(WasmCompiledModule, prev_instance)
  WCM_WEAK_LINK(WasmInstanceObject, owning_instance)

 public:
  static Handle<WasmCompiledModule> New(Isolate* isolate);

  static Handle<WasmCompiledModule> Clone(Isolate* isolate,
                                          Handle<WasmCompiledModule> module);
  static void Reset(Isolate* isolate, WasmCompiledModule* module);

  bool has_instance() const;

  void InsertInChain(WasmModuleObject*);
  void RemoveFromChain();

  DECL_ACCESSORS(raw_next_instance, Object);
  DECL_ACCESSORS(raw_prev_instance, Object);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(WasmCompiledModule);
};

class WasmDebugInfo : public Struct {
 public:
  DECL_ACCESSORS(wasm_instance, WasmInstanceObject)
  DECL_ACCESSORS(interpreter_handle, Object);
  DECL_ACCESSORS(interpreted_functions, Object);
  DECL_OPTIONAL_ACCESSORS(locals_names, FixedArray)
  DECL_OPTIONAL_ACCESSORS(c_wasm_entries, FixedArray)
  DECL_OPTIONAL_ACCESSORS(c_wasm_entry_map, Managed<wasm::SignatureMap>)

  DECL_CAST(WasmDebugInfo)

  // Dispatched behavior.
  DECL_PRINTER(WasmDebugInfo)
  DECL_VERIFIER(WasmDebugInfo)

// Layout description.
#define WASM_DEBUG_INFO_FIELDS(V)              \
  V(kInstanceOffset, kPointerSize)             \
  V(kInterpreterHandleOffset, kPointerSize)    \
  V(kInterpretedFunctionsOffset, kPointerSize) \
  V(kLocalsNamesOffset, kPointerSize)          \
  V(kCWasmEntriesOffset, kPointerSize)         \
  V(kCWasmEntryMapOffset, kPointerSize)        \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize, WASM_DEBUG_INFO_FIELDS)
#undef WASM_DEBUG_INFO_FIELDS

  static Handle<WasmDebugInfo> New(Handle<WasmInstanceObject>);

  // Setup a WasmDebugInfo with an existing WasmInstance struct.
  // Returns a pointer to the interpreter instantiated inside this
  // WasmDebugInfo.
  // Use for testing only.
  V8_EXPORT_PRIVATE static wasm::WasmInterpreter* SetupForTesting(
      Handle<WasmInstanceObject>);

  // Set a breakpoint in the given function at the given byte offset within that
  // function. This will redirect all future calls to this function to the
  // interpreter and will always pause at the given offset.
  static void SetBreakpoint(Handle<WasmDebugInfo>, int func_index, int offset);

  // Make a set of functions always execute in the interpreter without setting
  // breakpoints.
  static void RedirectToInterpreter(Handle<WasmDebugInfo>,
                                    Vector<int> func_indexes);

  void PrepareStep(StepAction);

  // Execute the specified function in the interpreter. Read arguments from
  // arg_buffer.
  // The frame_pointer will be used to identify the new activation of the
  // interpreter for unwinding and frame inspection.
  // Returns true if exited regularly, false if a trap occurred. In the latter
  // case, a pending exception will have been set on the isolate.
  bool RunInterpreter(Address frame_pointer, int func_index,
                      Address arg_buffer);

  // Get the stack of the wasm interpreter as pairs of <function index, byte
  // offset>. The list is ordered bottom-to-top, i.e. caller before callee.
  std::vector<std::pair<uint32_t, int>> GetInterpretedStack(
      Address frame_pointer);

  std::unique_ptr<wasm::InterpretedFrame, wasm::InterpretedFrameDeleter>
  GetInterpretedFrame(Address frame_pointer, int frame_index);

  // Unwind the interpreted stack belonging to the passed interpreter entry
  // frame.
  void Unwind(Address frame_pointer);

  // Returns the number of calls / function frames executed in the interpreter.
  uint64_t NumInterpretedCalls();

  // Get scope details for a specific interpreted frame.
  // This returns a JSArray of length two: One entry for the global scope, one
  // for the local scope. Both elements are JSArrays of size
  // ScopeIterator::kScopeDetailsSize and layout as described in debug-scopes.h.
  // The global scope contains information about globals and the memory.
  // The local scope contains information about parameters, locals, and stack
  // values.
  static Handle<JSObject> GetScopeDetails(Handle<WasmDebugInfo>,
                                          Address frame_pointer,
                                          int frame_index);
  static Handle<JSObject> GetGlobalScopeObject(Handle<WasmDebugInfo>,
                                               Address frame_pointer,
                                               int frame_index);
  static Handle<JSObject> GetLocalScopeObject(Handle<WasmDebugInfo>,
                                              Address frame_pointer,
                                              int frame_index);

  static Handle<JSFunction> GetCWasmEntry(Handle<WasmDebugInfo>,
                                          wasm::FunctionSig*);
};

#undef DECL_OPTIONAL_ACCESSORS
#undef WCM_CONST_OBJECT
#undef WCM_LARGE_NUMBER
#undef WCM_OBJECT
#undef WCM_OBJECT_OR_WEAK
#undef WCM_WEAK_LINK

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_WASM_WASM_OBJECTS_H_
