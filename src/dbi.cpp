
// This file is part of node-lmdb, the Node.js binding for lmdb
// Copyright (c) 2013 Timur Kristóf
// Licensed to you under the terms of the MIT license
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "node-lmdb.h"

using namespace v8;
using namespace node;

void setFlagFromValue(int *flags, int flag, const char *name, bool defaultValue, Local<Object> options);

DbiWrap::DbiWrap(MDB_env *env, MDB_dbi dbi) {
    this->needsClose = false;
    this->env = env;
    this->dbi = dbi;
}

DbiWrap::~DbiWrap() {
    // Close if not closed already
    if (needsClose) {
        mdb_dbi_close(env, dbi);
    }
}

Handle<Value> DbiWrap::ctor(const Arguments& args) {
    HandleScope scope;
    
    MDB_dbi dbi;
    MDB_txn *txn;
    int rc;
    int flags = 0;
    int keyIsUint32 = 0;
    char *cname = NULL;
    
    EnvWrap *ew = ObjectWrap::Unwrap<EnvWrap>(args[0]->ToObject());
    if (args[1]->IsObject()) {
        Local<Object> options = args[1]->ToObject();
        Local<String> name = options->Get(String::NewSymbol("name"))->ToString();
        
        int l = name->Length();
        cname = new char[l + 1];
        name->WriteAscii(cname);
        cname[l] = 0;
        
        // Get flags from options
        
        // NOTE: mdb_set_relfunc is not exposed because MDB_FIXEDMAP is "highly experimental"
        // NOTE: mdb_set_relctx is not exposed because MDB_FIXEDMAP is "highly experimental"
        setFlagFromValue(&flags, MDB_REVERSEKEY, "reverseKey", false, options);
        setFlagFromValue(&flags, MDB_DUPSORT, "dupSort", false, options);
        setFlagFromValue(&flags, MDB_DUPFIXED, "dupFixed", false, options);
        setFlagFromValue(&flags, MDB_INTEGERDUP, "integerDup", false, options);
        setFlagFromValue(&flags, MDB_REVERSEDUP, "reverseDup", false, options);
        setFlagFromValue(&flags, MDB_CREATE, "create", false, options);
            
        // TODO: wrap mdb_set_compare
        // TODO: wrap mdb_set_dupsort
        
        // See if key is uint32_t
        setFlagFromValue(&keyIsUint32, 1, "keyIsUint32", false, options);
        if (keyIsUint32) {
            flags |= MDB_INTEGERKEY;
        }
    }
    else {
        ThrowException(Exception::Error(String::New("Invalid parameters.")));
        return Undefined();
    }
    
    // Open transaction
    rc = mdb_txn_begin(ew->env, NULL, 0, &txn);
    if (rc != 0) {
        delete cname;
        mdb_txn_abort(txn);
        ThrowException(Exception::Error(String::New(mdb_strerror(rc))));
        return Undefined();
    }
    
    // Open database
    rc = mdb_dbi_open(txn, cname, flags, &dbi);
    delete cname;
    if (rc != 0) {
        mdb_txn_abort(txn);
        ThrowException(Exception::Error(String::New(mdb_strerror(rc))));
        return Undefined();
    }
    
    // Commit transaction
    rc = mdb_txn_commit(txn);
    if (rc != 0) {
        ThrowException(Exception::Error(String::New(mdb_strerror(rc))));
        return Undefined();
    }
    
    // Create wrapper
    DbiWrap* dw = new DbiWrap(ew->env, dbi);
    dw->needsClose = true;
    dw->Wrap(args.This());
    dw->keyIsUint32 = keyIsUint32;

    return args.This();
}

Handle<Value> DbiWrap::close(const Arguments& args) {
    HandleScope scope;
    
    DbiWrap *dw = ObjectWrap::Unwrap<DbiWrap>(args.This());
    mdb_dbi_close(dw->env, dw->dbi);
    dw->needsClose = false;
    
    return Undefined();
}

Handle<Value> DbiWrap::drop(const Arguments& args) {
    DbiWrap *dw = ObjectWrap::Unwrap<DbiWrap>(args.This());
    int del = 1;
    int rc;
    MDB_txn *txn;
    
    // Check if the database should be deleted
    if (args.Length() == 2 && args[1]->IsObject()) {
        Handle<Object> options = args[1]->ToObject();
        Handle<Value> opt = options->Get(String::NewSymbol("justFreePages"));
        del = opt->IsBoolean() ? !(opt->BooleanValue()) : 1;
    }
    
    // Begin transaction
    rc = mdb_txn_begin(dw->env, NULL, 0, &txn);
    if (rc != 0) {
        ThrowException(Exception::Error(String::New(mdb_strerror(rc))));
        return Undefined();
    }
    
    // Drop database
    rc = mdb_drop(txn, dw->dbi, del);
    if (rc != 0) {
        ThrowException(Exception::Error(String::New(mdb_strerror(rc))));
        return Undefined();
    }
    
    // Commit transaction
    rc = mdb_txn_commit(txn);
    if (rc != 0) {
        ThrowException(Exception::Error(String::New(mdb_strerror(rc))));
        return Undefined();
    }
    
    return Undefined();
}

