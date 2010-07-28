#include <iostream>

#include "ArrayObject.h"
#include "BlockObject.h"
#include "CodeBlock.h"
#include "Environment.h"
#include "FiberObject.h"
#include "IInterpreterHost.h"
#include "Interpreter.h"
#include "Fiber.h"

namespace Finch
{
    using std::cout;
    using std::endl;
    
    Fiber::Fiber(Interpreter & interpreter, Ref<Object> block)
    :   mIsRunning(false),
        mInterpreter(interpreter),
        mEnvironment(interpreter.GetEnvironment())
    {
        // push the starting block
        BlockObject * blockObj = block->AsBlock();
        mCallStack.Push(CallFrame(blockObj->Closure(), block));
        
        // if not in any method, self is Nil
        mReceivers.Push(mEnvironment.Nil());
    }

    bool Fiber::IsDone() const
    {
        return mCallStack.Count() == 0;
    }

    Ref<Object> Fiber::Execute()
    {
        mIsRunning = true;
        
        // continue processing bytecode until the entire callstack has completed
        // or this fiber gets paused (to switch to another fiber)
        while (mIsRunning && mCallStack.Count() > 0)
        {
            CallFrame & frame = mCallStack.Peek();
            const Instruction & instruction = frame.Block().GetCode()[frame.address];
            
            // advance past the instruction
            frame.address++;
            
            switch (instruction.op)
            {
                case OP_NOTHING:
                    // do nothing
                    break;
                    
                case OP_NUMBER_LITERAL:
                    PushOperand(Object::NewNumber(mEnvironment, instruction.arg.number));
                    break;
                    
                case OP_STRING_LITERAL:
                    {
                        String string = mEnvironment.Strings().Find(instruction.arg.id);
                        PushOperand(Object::NewString(mEnvironment, string));
                    }
                    break;
                    
                case OP_BLOCK_LITERAL:
                    {
                        // capture the current scope
                        Ref<Scope> closure = frame.scope;
                        
                        const CodeBlock & code = mEnvironment.Blocks().Find(instruction.arg.id);
                        Ref<Object> block = Object::NewBlock(mEnvironment, code,
                                closure, mReceivers.Peek());
                        
                        PushOperand(block);
                    }
                    break;
                    
                case OP_CREATE_ARRAY:
                    {
                        // create the array
                        Ref<Object> object = Object::NewArray(mEnvironment, 0);
                        ArrayObject * array = object->AsArray();
                        
                        // pop the elements
                        Array<Ref<Object> > elements;
                        for (int i = 0; i < instruction.arg.id; i++)
                        {
                            array->Elements().Add(PopOperand());
                        }
                        
                        // reverse them since the stack has them in order (so
                        // that elements are evaluated from left to right) and
                        // popping reverses the order
                        array->Elements().Reverse();
                        
                        // return the array
                        Push(object);
                    }
                    break;
                    
                case OP_POP:
                    PopOperand();
                    break;
                    
                case OP_DUP:
                    PushOperand(mOperands.Peek());
                    break;
                    
                case OP_DEF_GLOBAL:
                    {
                        // def returns the defined value, so instead of popping
                        // and then pushing the value back on the stack, we'll
                        // just peek
                        Ref<Object> value = mOperands.Peek();
                        //### bob: if we get strings fully interned (i.e. no dupes in
                        // string table), then the global name scope doesn't need the
                        // actual string at all, just the id in the string table
                        String name = mEnvironment.Strings().Find(instruction.arg.id);
                        mEnvironment.Globals()->Define(name, value);
                    }
                    break;
                    
                case OP_DEF_OBJECT:
                    {
                        // def returns the defined value, so instead of popping
                        // and then pushing the value back on the stack, we'll
                        // just peek
                        Ref<Object> value = mOperands.Peek();
                        String name = mEnvironment.Strings().Find(instruction.arg.id);
                        if (!Self().IsNull())
                        {
                            Ref<Object> self = Self();
                            Ref<Scope> scope = Self()->ObjectScope();
                            Self()->ObjectScope()->Define(name, value);
                        }
                    }
                    break;
                    
                case OP_DEF_LOCAL:
                    {
                        // def returns the defined value, so instead of popping
                        // and then pushing the value back on the stack, we'll
                        // just peek
                        Ref<Object> value = mOperands.Peek();
                        String name = mEnvironment.Strings().Find(instruction.arg.id);
                        CurrentScope()->Define(name, value);
                    }
                    break;
                    
                case OP_UNDEF_GLOBAL:
                    {
                        String name = mEnvironment.Strings().Find(instruction.arg.id);
                        mEnvironment.Globals()->Undefine(name);
                        PushNil();
                    }
                    break;
                    
                case OP_UNDEF_OBJECT:
                    {
                        String name = mEnvironment.Strings().Find(instruction.arg.id);
                        if (!Self().IsNull())
                        {
                            Self()->ObjectScope()->Undefine(name);
                        }
                        PushNil();
                    }
                    break;
                    
                case OP_UNDEF_LOCAL:
                    {
                        String name = mEnvironment.Strings().Find(instruction.arg.id);
                        CurrentScope()->Undefine(name);
                        PushNil();
                    }
                    break;
                    
                case OP_SET_LOCAL:
                    {
                        // set returns the defined value, so instead of popping
                        // and then pushing the value back on the stack, we'll
                        // just peek
                        Ref<Object> value = mOperands.Peek();
                        String name = mEnvironment.Strings().Find(instruction.arg.id);
                        CurrentScope()->Set(name, value);
                    }
                    break;
                    
                case OP_LOAD_GLOBAL:
                    {
                        String name = mEnvironment.Strings().Find(instruction.arg.id);
                        Ref<Object> value = mEnvironment.Globals()->LookUp(name);
                        PushOperand(value.IsNull() ? mEnvironment.Nil() : value);
                    }
                    break;
                    
                case OP_LOAD_OBJECT:
                    {
                        String name = mEnvironment.Strings().Find(instruction.arg.id);
                        if (!Self().IsNull())
                        {
                            Ref<Object> value = Self()->ObjectScope()->LookUp(name);
                            PushOperand(value.IsNull() ? mEnvironment.Nil() : value);
                        }
                        else
                        {
                            PushOperand(Ref<Object>());
                        }
                    }
                    break;
                    
                case OP_LOAD_LOCAL:
                    {
                        String name = mEnvironment.Strings().Find(instruction.arg.id);
                        Ref<Object> value = CurrentScope()->LookUp(name);
                        PushOperand(value.IsNull() ? mEnvironment.Nil() : value);
                    }
                    break;
                    
                case OP_LOAD_SELF:
                    PushOperand(Self());
                    break;
                    
                case OP_MESSAGE_0:
                case OP_MESSAGE_1:
                case OP_MESSAGE_2:
                case OP_MESSAGE_3:
                case OP_MESSAGE_4:
                case OP_MESSAGE_5:
                case OP_MESSAGE_6:
                case OP_MESSAGE_7:
                case OP_MESSAGE_8:
                case OP_MESSAGE_9:
                case OP_MESSAGE_10:
                    {
                        int numArgs = instruction.op - OP_MESSAGE_0;
                        
                        // pop the arguments. note that we fill the array from back to front
                        // because the arguments are on the stack from first to last (so that they
                        // were correctly evaluates from left to right) and now we're popping them
                        // off from last to first.
                        Array<Ref<Object> > args(numArgs, mEnvironment.Nil());
                        for (int i = numArgs - 1; i >= 0; i--)
                        {
                            args[i] = PopOperand();
                        }
                        
                        // send the message
                        String string = mEnvironment.Strings().Find(instruction.arg.id);
                        Ref<Object> receiver = PopOperand();
                        
                        receiver->Receive(receiver, *this, string, args);
                    }
                    break;
                                        
                case OP_END_BLOCK:
                    mCallStack.Pop();
                    mReceivers.Pop();
                    break;
                    
                default:
                    ASSERT(false, "Unknown op code.");
            }
        }
        
        // the last operation the fiber performed leaves its result on
        // the stack. that's the result of executing the fiber's block.
        if (IsDone())
        {
            return PopOperand();
        }
        
        return Ref<Object>();
    }

    void Fiber::Push(Ref<Object> object)
    {
        PushOperand(object);
    }
    
    void Fiber::PushNil()
    {
        Push(mEnvironment.Nil());
    }

    void Fiber::PushBool(bool value)
    {
        PushOperand(value ? mEnvironment.True() : mEnvironment.False());
    }

    void Fiber::PushNumber(double value)
    {
        Push(Object::NewNumber(mEnvironment, value));
    }

    void Fiber::PushString(const String & value)
    {
        Push(Object::NewString(mEnvironment, value));
    }

    void Fiber::CallMethod(Ref<Object> self, Ref<Object> blockObj,
                                 const Array<Ref<Object> > & args)
    {
        BlockObject & block = *(blockObj->AsBlock());
        
        // create a new local scope for the block
        Ref<Scope> scope = Ref<Scope>(new Scope(block.Closure()));
        
        // bind the arguments to the parameters. missing arguments will be
        // filled with Nil, and extra arguments will be ignored.
        for (int i = 0; i < block.Params().Count(); i++)
        {
            Ref<Object> arg = (i < args.Count()) ? args[i] : mEnvironment.Nil();
            scope->Define(block.Params()[i], arg);
        }
        
        // perform tail call optimization. we've already advanced the
        // instruction on the current call frame to the next instruction. before
        // we push a new callframe, we'll check if the current callframe is now
        // done. if so, we can discard it now instead of waiting for the new
        // callframe to return to it.
        CallFrame & frame = mCallStack.Peek();
        const Instruction & instruction = frame.Block().GetCode()[frame.address];
        if (instruction.op == OP_END_BLOCK)
        {
            mCallStack.Pop();
            mReceivers.Pop();
        }

        // push the call onto the stack
        mCallStack.Push(CallFrame(scope, blockObj));
        mReceivers.Push(self);
    }
    
    void Fiber::CallBlock(Ref<Object> blockObj,
                                const Array<Ref<Object> > & args)
    {
        BlockObject & block = *(blockObj->AsBlock());
        CallMethod(block.Self(), blockObj, args);
    }

    void Fiber::Error(const String & message)
    {
        mInterpreter.GetHost().Error(message);
    }
    
    int Fiber::GetCallstackDepth() const
    {
        return mCallStack.Count();
    }
    
    Ref<Object> Fiber::Self()
    {
        return mReceivers.Peek();
    }
    
    void Fiber::PushOperand(Ref<Object> object)
    {
        ASSERT(!object.IsNull(), "Cannot push a null object. (Should be Nil instead.)");
        
        //std::cout << "push " << object << std::endl;
        mOperands.Push(object);
    }
    
    Ref<Object> Fiber::PopOperand()
    {
        Ref<Object> object = mOperands.Pop();
        
        //std::cout << "pop  " << object << std::endl;
        return object;
    }
}
