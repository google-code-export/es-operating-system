/*
 * Copyright 2008 Google Inc.
 * Copyright 2006, 2007 Nintendo Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <new>
#include <es.h>
#include <es/broker.h>
#include <es/exception.h>
#include <es/handle.h>
#include <es/reflect.h>
#include <es/variant.h>
#include "core.h"
#include "interfaceStore.h"
#include "process.h"

typedef long long (*Method)(void* self, ...);

Broker<Process::upcall, Process::INTERFACE_POINTER_MAX> Process::broker;
UpcallProxy Process::upcallTable[Process::INTERFACE_POINTER_MAX];

bool UpcallProxy::set(Process* process, void* object, const Guid& iid, bool used)
{
    if (ref.addRef() != 1)
    {
        ref.release();
        return false;
    }
    this->object = object;
    this->iid = iid;
    this->process = process;
    use.exchange(used ? 1 : 0);
    return true;
}

unsigned int UpcallProxy::addRef()
{
    return ref.addRef();
}

unsigned int UpcallProxy::release()
{
    return ref.release();
}

bool UpcallProxy::isUsed()
{
    return (use == 0 && use.increment() == 1) ? false : true;
}

int Process::
set(Process* process, void* object, const Guid& iid, bool used)
{
    for (UpcallProxy* proxy(upcallTable);
         proxy < &upcallTable[INTERFACE_POINTER_MAX];
         ++proxy)
    {
        if (proxy->set(process, object, iid, used))
        {
#ifdef VERBOSE
            esReport("Process::set(%p, {%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}) : %d;\n",
                     object,
                     iid.Data1, iid.Data2, iid.Data3,
                     iid.Data4[0], iid.Data4[1], iid.Data4[2], iid.Data4[3],
                     iid.Data4[4], iid.Data4[5], iid.Data4[6], iid.Data4[7],
                     proxy - upcallTable);
#endif
            return proxy - upcallTable;
        }
    }
    return -1;
}

long long Process::
upcall(void* self, void* base, int methodNumber, va_list ap)
{
    Thread* current = Thread::getCurrentThread();

    unsigned interfaceNumber = static_cast<void**>(self) - static_cast<void**>(base);
    Variant* variant = 0;
    if (INTERFACE_POINTER_MAX <= interfaceNumber)
    {
        // self must be a pointer to a Variant value for the method returns a Variant.
        variant = reinterpret_cast<Variant*>(self);
        self = va_arg(ap, void*);
        interfaceNumber = static_cast<void**>(self) - static_cast<void**>(base);
    }

    UpcallProxy* proxy = &upcallTable[interfaceNumber];

    // Now we need to identify which process is to be used for this upcall.
    Process* server = proxy->process;
    bool log = server->log;

#ifdef VERBOSE
    esReport("Process(%p)::upcall[%d] %d\n", server, interfaceNumber, methodNumber);
#endif

    // Determine the type of interface and which method is being invoked.
    Reflect::Interface interface = getInterface(proxy->iid);   // XXX Should cache the result

    // If this interface inherits another interface,
    // methodNumber is checked accordingly.
    if (interface.getInheritedMethodCount() + interface.getMethodCount() <= methodNumber)
    {
        throw SystemException<ENOSYS>();
    }
    unsigned baseMethodCount;
    Reflect::Interface super(interface);
    for (;;)
    {
        baseMethodCount = super.getInheritedMethodCount();
        if (baseMethodCount <= methodNumber)
        {
            break;
        }
        super = getInterface(super.getSuperIid());
    }
    Reflect::Method method(Reflect::Method(super.getMethod(methodNumber - baseMethodCount)));

    if (log)
    {
        esReport("upcall[%d:%p]: %s::%s(",
                 interfaceNumber, server, interface.getName(), method.getName());
    }

    unsigned long ref;
    if (super.getIid() == IInterface::iid())
    {
        switch (methodNumber - baseMethodCount)
        {
        case 1: // addRef
            ref = proxy->addRef();
            // If this is the first addRef() call to proxy,
            // redirect the call to the original object.
            if (proxy->isUsed())
            {
                if (log)
                {
                    esReport(") : %d/%d;\n", ref, (long) proxy->use);
                }
                return ref;
            }
            break;
        case 2: // release
            if (1 < proxy->ref || !proxy->use)
            {
                ref = proxy->release();
                if (log)
                {
                    esReport(") : %d/%d;\n", ref, (long) proxy->use);
                }
                return ref;
            }
            break;
        }
    }

    // Get a free upcall record
    UpcallRecord* record = server->getUpcallRecord();
    if (!record)
    {
        throw SystemException<ENOMEM>();
    }
    record->method = method;

    // Save the upcall context
    record->client = getCurrentProcess();
    record->proxy = proxy;
    record->methodNumber = methodNumber;
    record->param = ap;
    record->variant = variant;

    long long result(0);
    int errorCode(0);
    switch (record->getState())
    {
    case UpcallRecord::INIT:
        // Leap into the server process.
        current->leapIntoServer(record);

        // Initialize TLS.
        memmove(reinterpret_cast<void*>(record->ureg.esp),
                server->tlsImage, server->tlsImageSize);
        memset(reinterpret_cast<u8*>(record->ureg.esp) + server->tlsImageSize,
               0, server->tlsSize - server->tlsImageSize);

        record->push(0);                                            // param
        record->push(reinterpret_cast<unsigned>(server->focus));    // start
        record->push(0);                                            // ret
        record->entry(reinterpret_cast<unsigned>(server->startup));

        if (record->label.set() == 0)
        {
            // Make an upcall the server process.
            unsigned x = Core::splHi();
            Core* core = Core::getCurrentCore();
            record->sp0 = core->tss0->sp0;
            core->tss0->sp0 = current->sp0 = record->label.esp;
            Core::splX(x);
            record->ureg.load();
            // NOT REACHED HERE
        }
        else
        {
            // Switch the record state to READY.
            record->setState(UpcallRecord::READY);

            // Return to the client process.
            Process* client = current->returnToClient();
        }
        // FALL THROUGH
    case UpcallRecord::READY:
        // Copy parameters to the user stack of the server process
        errorCode = server->copyIn(record);
        if (errorCode)
        {
            break;
        }

        // Leap into the server process.
        current->leapIntoServer(record);

        // Invoke method
        record->ureg.eax = reinterpret_cast<u32>(record->proxy->object);
        record->ureg.edx = record->methodNumber;
        if (log)
        {
            esReport("object: %p, method: %d @ %p:%p\n", record->ureg.eax, record->ureg.edx, record->ureg.eip, record->ureg.esp);
        }
        if (record->label.set() == 0)
        {
            // Make an upcall the server process.
            unsigned x = Core::splHi();
            Core* core = Core::getCurrentCore();
            record->sp0 = core->tss0->sp0;
            core->tss0->sp0 = current->sp0 = record->label.esp;
            Core::splX(x);
            record->ureg.load();
            // NOT REACHED HERE
        }
        else
        {
            Guid iid = IInterface::iid();

            // Return to the client process.
            Process* client = current->returnToClient();

            // Copy output parameters from the user stack of the server process.
            errorCode = server->copyOut(record, iid);

            // Get result code
            if (errorCode == 0)
            {
                result = (static_cast<long long>(record->ureg.edx) << 32) | record->ureg.eax;
                errorCode = record->ureg.ecx;
            }

            if (errorCode == 0)
            {
                void** ip = 0;
                Reflect::Type returnType(record->method.getReturnType());
                switch (returnType.getType())
                {
                case Ent::SpecVariant:
                    if (record->variant->getType() == Variant::TypeObject)
                    {
                        ip = reinterpret_cast<void**>(static_cast<IInterface*>(*(record->variant)));
                    }
                    result = reinterpret_cast<intptr_t>(record->variant);
                    break;
                case Ent::TypeInterface:
                    iid = returnType.getInterface().getIid();
                    // FALL THROUGH
                case Ent::SpecObject:
                    // Convert the received interface pointer to kernel's interface pointer
                    ip = reinterpret_cast<void**>(result);
                    if (ip == 0)
                    {
                        result = 0;
                    }
                    break;
                }

                // Process the returned interface pointer
                if (ip) {
                    if (server->ipt <= ip && ip < server->ipt + INTERFACE_POINTER_MAX)
                    {
                        unsigned interfaceNumber(ip - server->ipt);
                        Handle<SyscallProxy> proxy(&server->syscallTable[interfaceNumber], true);
                        if (proxy->isValid())
                        {
                            result = reinterpret_cast<long>(proxy->getObject());
                        }
                        else
                        {
                            result = 0;
                            errorCode = EBADFD;
                        }
                    }
                    else if (server->isValid(ip, sizeof(void*)))
                    {
                        // Allocate an entry in the upcall table and set the
                        // interface pointer to the broker for the upcall table.
                        int n = set(server, reinterpret_cast<IInterface*>(ip), iid, true);
                        if (0 <= n)
                        {
                            result = reinterpret_cast<long>(&(broker.getInterfaceTable())[n]);
                        }
                        else
                        {
                            // XXX object pointed by ip would be left allocated.
                            result = 0;
                            errorCode = ENFILE;
                        }
                        if (log)
                        {
                            esReport(" = %p", ip);
                        }
                    }
                    else
                    {
                        errorCode = EBADFD;
                    }
                }
            }
        }
        break;
    }

    if (super.getIid() == IInterface::iid())
    {
        switch (methodNumber - baseMethodCount)
        {
        case 1: // addRef
            result = ref;
            break;
        case 2: // release
            result = proxy->release();
            break;
        }
    }

    if (errorCode)
    {
        // Switch the record state back to INIT.
        record->setState(UpcallRecord::INIT);
        server->putUpcallRecord(record);
        esThrow(errorCode);
    }

    server->putUpcallRecord(record);

    return result;
}

void Process::
returnFromUpcall(Ureg* ureg)
{
    Thread* current(Thread::getCurrentThread());
    UpcallRecord* record(current->upcallList.getLast());
    if (!record)
    {
        return; // Invalid return from upcall
    }
    ASSERT(record->process == getCurrentProcess());

    unsigned x = Core::splHi();
    Core* core = Core::getCurrentCore();
    core->tss0->sp0 = current->sp0 = record->sp0;
    Core::splX(x);
    memmove(&record->ureg, ureg, sizeof(Ureg));
    record->label.jump();
    // NOT REACHED HERE
}

void Process::
// setFocus(void* (*focus)(void* param)) // [check] focus must be a function pointer.
setFocus(const void* focus)
{
    typedef void* (*Focus)(void* param); // [check]
    this->focus = reinterpret_cast<Focus>(focus); // [check]
}

UpcallRecord* Process::
createUpcallRecord(const unsigned stackSize)
{
    // Map a user stack
    void* userStack(static_cast<u8*>(USER_MAX) - ((threadCount + upcallCount + 1) * stackSize));
    userStack = map(userStack, stackSize - Page::SIZE,
                    ICurrentProcess::PROT_READ | ICurrentProcess::PROT_WRITE,
                    ICurrentProcess::MAP_PRIVATE, 0, 0);
    if (!userStack)
    {
        return 0;
    }

    UpcallRecord* record = new(std::nothrow) UpcallRecord(this);
    if (!record)
    {
        unmap(userStack, stackSize - Page::SIZE);
        return 0;
    }

    Ureg* ureg = &record->ureg;
    memset(ureg, 0, sizeof(Ureg));
    ureg->gs = Core::TCBSEL;
    ureg->fs = ureg->es = ureg->ds = ureg->ss = Core::UDATASEL;
    ureg->cs = Core::UCODESEL;
    ureg->eflags = 0x0202;  // IF
    ureg->esp = reinterpret_cast<unsigned>(static_cast<u8*>(userStack) + stackSize - Page::SIZE);
    record->tls(tlsSize, tlsAlign);
    record->userStack = userStack;

    upcallCount.increment();

    return record;
}

UpcallRecord* Process::
getUpcallRecord()
{
    {
        Lock::Synchronized method(spinLock);

        UpcallRecord* record = upcallList.removeFirst();
        if (record)
        {
            return record;
        }
    }

    const unsigned stackSize = 2*1024*1024;
    return createUpcallRecord(stackSize);
}

void Process::
putUpcallRecord(UpcallRecord* record)
{
    Lock::Synchronized method(spinLock);

    upcallList.addLast(record);
}

u8* Process::
copyInString(const char* string, u8* esp)
{
    // Check zero termination
    const char* ptr = string;
    int count = 0;
    do
    {
        if (!isValid(ptr, 1))
        {
            return NULL;
        }
        ++count;
    } while (*ptr++);
    esp -= (count + sizeof(int) - 1) & ~(sizeof(int) - 1);
    write(string, count, reinterpret_cast<long long>(esp));
    return esp;
}

IInterface* Process::
copyInObject(IInterface* object, Guid& iid)
{
    if (!object)
    {
        return object;
    }

    void** ip = reinterpret_cast<void**>(object);
    int n = broker.getInterfaceNo(reinterpret_cast<void*>(ip));
    if (0 <= n)
    {
        UpcallProxy* proxy = &upcallTable[n];
        if (proxy->process == this)
        {
            return static_cast<IInterface*>(proxy->object);
        }
    }

    // Set up a new system call proxy.
    n = set(syscallTable, object, iid);
    if (0 <= n)
    {
        // Set ip to proxy ip
        ip = &ipt[n];
        object->addRef();
    }
    else
    {
        ip = 0; // XXX should raise an exception
    }
    // Note the reference count to the created syscall proxy must
    // be decremented by one at the end of this upcall.
    return reinterpret_cast<IInterface*>(ip); // XXX
}

// Note copyIn() is called against the server process, so that
// copyIn() can be called from the kernel thread to make an upcall,
int Process::
copyIn(UpcallRecord* record)
{
    UpcallProxy* proxy(record->proxy);
    int methodNumber(record->methodNumber);
    va_list paramv(record->param);
    int* paramp = reinterpret_cast<int*>(paramv);
    u8* esp(reinterpret_cast<u8*>(record->ureg.esp));
    Ureg* ureg(&record->ureg);

    int argv[sizeof(long long) / sizeof(int) * 9]; // XXX 9
    int argc = 0;
    int* argp = argv;

    int count = 0;

    Guid iid = IInterface::iid();

    Reflect::Type returnType = record->method.getReturnType();
    if (returnType.getType() == Ent::SpecVariant)
    {
        // Make space for the return value of Variant type
        count = sizeof(Variant);
        esp -= count;
        *argp++ = (int) esp;
    }

    // Set this
    *argp++ = (int) proxy->object;

    switch (returnType.getType())
    {
    case Ent::SpecVariant:
        // Variant op(void* buf, int len);
    case Ent::SpecString:
        // int op(char* buf, int len, ...);
        count = paramp[1];                          // XXX check count
        esp -= (count + sizeof(int) - 1) & ~(sizeof(int) - 1);
        *argp++ = (int) esp;
        *argp++ = count;
        paramp += 2;
        break;
    case Ent::SpecWString:
        // int op(wchar_t* buf, int len, ...);
        count = sizeof(wchar_t) * paramp[1];        // XXX check count
        esp -= (count + sizeof(int) - 1) & ~(sizeof(int) - 1);
        *argp++ = (int) esp;
        *argp++ = paramp[1];
        paramp += 2;
        break;
    case Ent::TypeSequence: // XXX
        // int op(xxx* buf, int len, ...);
        count = returnType.getSize() * paramp[1];   // XXX check count
        esp -= (count + sizeof(int) - 1) & ~(sizeof(int) - 1);
        *argp++ = (int) esp;
        *argp++ = paramp[1];
        paramp += 2;
        break;
    case Ent::SpecUuid:
    case Ent::TypeStructure:
    case Ent::TypeArray:
        // void op(struct* buf, ...);
        // void op(xxx[x] buf, ...);
        count = returnType.getSize();               // XXX check count
        esp -= (count + sizeof(int) - 1) & ~(sizeof(int) - 1);
        *argp++ = (int) esp;
        ++paramp;
        break;
    default:
        break;
    }

    for (int i = 0; i < record->method.getParameterCount(); ++i)
    {
        Reflect::Parameter param(record->method.getParameter(i));
        Reflect::Type type(param.getType());
        if (log)
        {
            esReport("%s ", param.getName());
        }

        switch (type.getType())
        {
        case Ent::SpecVariant:
            {
                Variant* var = reinterpret_cast<Variant*>(paramp);
                switch (var->getType())
                {
                case Variant::TypeString:
                    esp = copyInString(static_cast<const char*>(*var), esp);
                    if (!esp)
                    {
                        return EFAULT;
                    }
                    *var = Variant(reinterpret_cast<const char*>(esp));
                    break;
                case Variant::TypeObject:
                    *var = Variant(reinterpret_cast<IInterface*>(copyInObject(static_cast<IInterface*>(*var), iid)));
                    break;
                }
                memcpy(argp, paramp, sizeof(VariantBase));
                argp += sizeof(VariantBase) / sizeof(int);
                paramp += sizeof(VariantBase) / sizeof(int);
                var->makeVariant();
            }
            break;
        case Ent::SpecAny:  // XXX x86 specific
        case Ent::SpecBool:
        case Ent::SpecChar:
        case Ent::SpecWChar:
        case Ent::SpecS8:
        case Ent::SpecS16:
        case Ent::SpecS32:
        case Ent::SpecU8:
        case Ent::SpecU16:
        case Ent::SpecU32:
        case Ent::SpecF32:
            *argp++ = *paramp++;
            break;
        case Ent::SpecS64:
        case Ent::SpecU64:
        case Ent::SpecF64:
            *argp++ = *paramp++;
            *argp++ = *paramp++;
            break;
        case Ent::SpecString:
            esp = copyInString(*reinterpret_cast<char**>(paramp), esp);
            if (!esp)
            {
                return EFAULT;
            }
            *argp++ = (int) esp;
            ++paramp;
            break;
        case Ent::TypeSequence:
            // xxx* buf, int len, ...
            count = type.getSize() * paramp[1]; // XXX check count
            esp -= (count + sizeof(int) - 1) & ~(sizeof(int) - 1);
            write(*reinterpret_cast<void**>(paramp), count, reinterpret_cast<long long>(esp));
            *argp++ = (int) esp;
            *argp++ = paramp[1];
            paramp += 2;
            break;
        case Ent::SpecUuid:         // Guid* guid, ...
            iid = **reinterpret_cast<Guid**>(paramp);
            // FALL THROUGH
        case Ent::TypeStructure:    // struct* buf, ...
        case Ent::TypeArray:        // xxx[x] buf, ...
            count = type.getSize();
            esp -= (count + sizeof(int) - 1) & ~(sizeof(int) - 1);
            write(*reinterpret_cast<void**>(paramp), count, reinterpret_cast<long long>(esp));
            *argp++ = (int) esp;
            ++paramp;
            break;
        case Ent::TypeInterface:
            iid = type.getInterface().getIid();
            // FALL THROUGH
        case Ent::SpecObject:
            *argp++ = *paramp = (int) copyInObject(*reinterpret_cast<IInterface**>(paramp), iid);
            // Note the reference count to the created syscall proxy must
            // be decremented by one at the end of this upcall.
            ++paramp;
            break;
        default:
            break;
        }

        if (log && i + 1 < record->method.getParameterCount())
        {
            esReport(", ");
        }
    }
    if (log)
    {
        esReport(");\n");
    }

    // Copy arguments
    esp -= sizeof(int) * (argp - argv);
    write(argv, sizeof(int) * (argp - argv), reinterpret_cast<long>(esp));
    ureg->esp = reinterpret_cast<long>(esp);

    return 0;
}

// Note copyOut() is called against the server process.
int Process::
copyOut(UpcallRecord* record, Guid& iid)
{
    UpcallProxy* proxy(record->proxy);
    int methodNumber(record->methodNumber);
    void* paramv(record->param);
    int* paramp = reinterpret_cast<int*>(paramv);
    u8* esp(reinterpret_cast<u8*>(record->ureg.esp));  // XXX should be saved separately

    long long rc = (static_cast<long long>(record->ureg.edx) << 32) | record->ureg.eax;

    if (log)
    {
        Reflect::Interface interface;
        try
        {
            interface = getInterface(proxy->iid);
        }
        catch (Exception& error)
        {
            return error.getResult();
        }
        esReport("return from upcall %p: %s::%s(",
                 esp, interface.getName(), record->method.getName());
    }

    int count;

    Reflect::Type returnType = record->method.getReturnType();
    if (returnType.getType() == Ent::SpecVariant)
    {
        count = sizeof(Variant);
        esp -= count;
        read(record->variant, count, reinterpret_cast<long long>(esp));
    }
    switch (returnType.getType())
    {
    case Ent::SpecVariant:
        // Variant op(void* buf, int len);
        rc = paramp[1]; // XXX check type and string length if type is string
        // FALL THROUGH
    case Ent::SpecString:
        // int op(char* buf, int len, ...);
        count = paramp[1];
        esp -= (count + sizeof(int) - 1) & ~(sizeof(int) - 1);
        if (0 < rc)
        {
            // XXX check string length
            read(*reinterpret_cast<void**>(paramp), count, reinterpret_cast<long long>(esp));
        }
        paramp += 2;
        break;
    case Ent::SpecWString:
        // int op(wchar_t* buf, int len, ...);
        count = sizeof(wchar_t) * paramp[1];
        esp -= (count + sizeof(int) - 1) & ~(sizeof(int) - 1);
        if (0 < rc)
        {
            read(*reinterpret_cast<void**>(paramp), count, reinterpret_cast<long long>(esp));
        }
        paramp += 2;
        break;
    case Ent::TypeSequence:
        // int op(xxx* buf, int len, ...);
        count = returnType.getSize() * paramp[1];
        esp -= (count + sizeof(int) - 1) & ~(sizeof(int) - 1);
        if (0 < rc && rc <= paramp[1])
        {
            read(*reinterpret_cast<void**>(paramp), returnType.getSize() * rc, reinterpret_cast<long long>(esp));
        }
        paramp += 2;
        break;
    case Ent::SpecUuid:
    case Ent::TypeStructure:
    case Ent::TypeArray:
        // void op(struct* buf, ...);
        // void op(xxx[x] buf, ...);
        count = returnType.getSize();   // XXX check count
        esp -= (count + sizeof(int) - 1) & ~(sizeof(int) - 1);
        read(*reinterpret_cast<void**>(paramp), count, reinterpret_cast<long long>(esp));
        ++paramp;
        break;
    default:
        break;
    }

    int argc = 0;
    for (int i = 0; i < record->method.getParameterCount(); ++i)
    {
        Reflect::Parameter param(record->method.getParameter(i));
        Reflect::Type type(param.getType());
        if (log)
        {
            esReport("%s", param.getName());
        }

        void** ip = 0;
        switch (type.getType())
        {
        case Ent::SpecVariant:
            {
                Variant* var = reinterpret_cast<Variant*>(paramp);
                if (var->getType() == Variant::TypeObject)
                {
                    ip = reinterpret_cast<void**>(static_cast<IInterface*>(*var));
                }
                paramp += sizeof(VariantBase) / sizeof(int);
            }
            break;
        case Ent::SpecAny:  // XXX x86 specific
        case Ent::SpecBool:
        case Ent::SpecChar:
        case Ent::SpecWChar:
        case Ent::SpecS8:
        case Ent::SpecS16:
        case Ent::SpecS32:
        case Ent::SpecU8:
        case Ent::SpecU16:
        case Ent::SpecU32:
        case Ent::SpecF32:
            ++paramp;
            break;
        case Ent::SpecS64:
        case Ent::SpecU64:
        case Ent::SpecF64:
            paramp += 2;
            break;
        case Ent::SpecString:
            ++paramp;
            break;
        case Ent::SpecWString:
            ++paramp;
            break;
        case Ent::TypeSequence:
            // xxx* buf, int len, ...
            count = type.getSize() * paramp[1];
            esp -= (count + sizeof(int) - 1) & ~(sizeof(int) - 1);
            paramp += 2;
            break;
        case Ent::SpecUuid:         // Guid* guid, ...
            iid = **reinterpret_cast<Guid**>(paramp);
            // FALL THROUGH
        case Ent::TypeStructure:    // struct* buf, ...
        case Ent::TypeArray:        // xxx[x] buf, ...
            count = type.getSize();
            esp -= (count + sizeof(int) - 1) & ~(sizeof(int) - 1);
            ++paramp;
            break;
        case Ent::SpecObject:
        case Ent::TypeInterface:
            ip = *reinterpret_cast<void***>(paramp);
            ++paramp;
            break;
        default:
            break;
        }

        if (ip && ipt <= ip && ip < ipt + INTERFACE_POINTER_MAX)
        {
            // Release the created syscall proxy.
            SyscallProxy* proxy(&syscallTable[ip - ipt]);
            proxy->release();
        }

        if (log && i + 1 < record->method.getParameterCount())
        {
            esReport(", ");
        }
    }
    if (log)
    {
        esReport(");\n");
    }

    return 0;
}