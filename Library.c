
// creates idle task when nothing else is happening
static void task_idle(void)
{
    while (1)
        ;
}

exception init_kernel()
{
    set_ticks(0);
    KernelMode = INIT;

    //Create 3 linked lists
    TimerList = (list *)newList();
    if (TimerList == NULL)
    {
        return FAIL;
    }
    ReadyList = (list *)newList();
    if (ReadyList == NULL)
    {
        return FAIL;
    }
    WaitingList = (list *)newList();
    if (WaitingList == NULL)
    {
        return FAIL;
    }
    //create an idle task with infinite deadline
    create_task(&task_idle, UINT_MAX);
    return OK;
}

exception create_task(void (*body)(), uint d)
{

    TCB *new_tcb = (TCB *)calloc(1, sizeof(TCB));
    // make sure Calloc was successful
    if (new_tcb == NULL)
    {
        return FAIL;
    }
    new_tcb->PC = body;
    new_tcb->SPSR = 0x21000000;
    new_tcb->Deadline = d;

    new_tcb->StackSeg[STACK_SIZE - 2] = 0x21000000;
    new_tcb->StackSeg[STACK_SIZE - 3] = (unsigned int)body;
    new_tcb->SP = &(new_tcb->StackSeg[STACK_SIZE - 9]);

    if (KernelMode == INIT)
    {

        insertR(ReadyList, new_tcb)
        {
            free(new_tcb);

            return SUCCESS;
        }
        else
        {
            // disable interrupts
            isr_off();
            PreviousTask = NextTask;
            sort(ReadyList);

            insertR(ReadyList, new_tcb)
                NextTask = ReadyList->pHead->pNext->pTask;
            SwitchContext();
            // update running pointer
            Running = readylist->pHead->pNext->pTask;
        }

        return OK;
    }

    void run()
    {
        // Initialize interrupt timer here
        set_ticks(0);
        KernelMode = RUNNING;
        // set Running to point to the task with the tightest deadline
        NextTask = readylist->pHead->pNext->pTask;
        LoadContext_In_Run();
    }

    void terminate()
    {
        isr(off);
        // terminate can only be called by the running task, which is in
        // the first position of ready list
        removeFirst(readylist);
        NextTask = readylist->pHead->pNext->pTask;
        switch_to_stack_of_next_stack();
        // go to the next task
        // loadcontext reenables interupts
        LoadContext_In_Terminate();
    }

    //
    mailbox *create_mailbox(uint nMessages, uint nDataSize)
    {
        mailbox *mBox = (mailbox *)malloc(1 * sizeof(mailbox));
        if (mBox == NULL)
        {
            return NULL;
        }
        mBox->pHead = (msg *)malloc(1 * sizeof(msg));
        if (mBox->pHead == NULL)
        {
            free(mBox);
            return NULL;
        }
        mBox->pTail = (msg *)malloc(1 * sizeof(msg));
        if (mBox->pTail == NULL)
        {
            free(mBox->pHead);
            free(mBox);
            return NULL;
        }
        mBox->pHead->pNext = NULL;
        mBox->pTail->pPrevious = NULL;
        mBox->nDataSize = nDataSize;
        mBox->nMaxMessages = nMessages;
        mBox->nMessages = 0;
        mBox->nBlockedMsg = 0;
        return mBox;
    }

    exception no_messages(mailbox * mBox)
    {
        if (mBox->nMessages == 0 && mBox->nBlockedMsg == 0)
        {
            free(mBox->pHead);
            free(mBox->pTail);
            free(mBox);
            return OK;
        }
        else
        {
            return NOT_EMPTY;
        }
    }

    int send_wait(mailbox * mBox, void *pData)
    {
        isr_off();
        // if recieveing task is waiting
        if (no_message(mBox) != 0 && mBox->pHead->pNext->Status == RECEIVER)
        {
            memcpy(mBox->pHead->pNext->pData, pData, mBox->nDataSize);
            mBox->pHead->pNext->pBlock->pMessage = NULL;
            move(mBox->pHead->pNext->pBlock,
                 waitinglist, readylist, 1);
            // update running pointer
            nextTask = readylist->pHead->pNext->pTask;
            removeFirstMsg(mBox);
        }
        else
        {
            msg *tmpMsg(msg *) malloc(sizeof(msg));
            if (tmpMsg == NULL)
            {
                return FAIL;
            }

            // initialize values in this message
            tmpMsg->pData = pData;
            tmpMsg->Status = SENDER;
            tmpMsg->pBlock = readylist->pHead->pNext;
            // associate the message to the object
            readylist->pHead->pNext->pMessage = tmpMsg;

            appendMsg(mBox, tmpMsg, 0);
            moveFirst(readylist, waitinglist, 0);
            // update running pointer
            PreviousTask = NextTask;
            sort(ReadyList);
            NextTask = readylist->pHead->pNext->pTask;
        }
        SwitchContext();

        if (Ticks >= NextTask->DeadLine)
        {
            isr_off();
            msg *tmpMsg = readylist->pHead->pNext->pMessage;
            removeMsg(mBox, tmpMsg);
            readylist->pHead->pNext->pMessage = NULL;
            isr_on();
            return = DEADLINE_REACHED;
        }
        else
        {
            return = OK;
        }
    }

    exception receive_wait(mailbox * mBox, void *pData)
    {
        isr_off();
        // if send message is waiting
        if (no_message(mBox) != 0 && mBox->pHead->pNext->Status == SENDER)
        {
            // copy sender data and remove sending task from mailbox.
            memcpy(pData, mBox->pHead->pNext->pData, mBox->nDataSize);
            mBox->pHead->pNext->pBlock->pMessage = NULL;

            if (mBox->nBlockedMsg != 0)
            {
                // this is a block message
                // cut the relation between the task and the message
                // update NextTask pointer
                PreviousTask = NextTask;
                move(mBox->pHead->pNext->pBlock, waitinglist, readylist, 1);
                NextTask = readylist->pHead->pNext->pTask;
                // removeFirstMsg(mBox);
            }
            else
            {
                // this is an unblocked message
                removeFirstMsg(mBox);
            }
        }
        else
        {
            msg *tmpMsg;
            // we need to put one mail in this mailbox
            // we assume that there is enough room this message
            // the new message
            tmpMsg = (msg *)malloc(sizeof(msg));
            if (tmpMsg == NULL)
            {
                return FAIL;
            }
            // initialize values in this message
            tmpMsg->pData = Data;
            tmpMsg->Status = RECEIVER;
            tmpMsg->pBlock = readylist->pHead->pNext;
            // associate the message to the object
            readylist->pHead->pNext->pMessage = tmpMsg;

            appendMsg(mBox, tmpMsg, 0);
            moveFirst(readylist, waitinglist, 0);
            // update running pointer
            NextTask = readylist->pHead->pNext->pTask;
        }
    }
    else
    {
        if (Ticks >= Running->DeadLine)
        {
            msg *tmpMsg = readylist->pHead->pNext->pMessage;
            removeMsg(mBox, tmpMsg);
            readylist->pHead->pNext->pMessage = NULL;
            returnValue = DEADLINE_REACHED;
        }
        else
        {
            returnValue = OK;
        }
    }
    return returnValue;
}

exception send_no_wait(mailbox *mBox, void *Data)
{
    volatile int first = 1;
    isr_off();
    SaveContext();
    if (first)
    {
        first = 0;
        if (no_message(mBox) != 0 && mBox->pHead->pNext->Status == RECEIVER)
        {
            memcpy(mBox->pHead->pNext->pData, Data, mBox->nDataSize);
            // this must be a block message
            // cut the relation between the task and the message
            mBox->pHead->pNext->pBlock->pMessage = NULL;
            move(mBox->pHead->pNext->pBlock,
                 waitinglist, readylist, 1);
            // update running pointer
            Running = readylist->pHead->pNext->pTask;
            removeFirstMsg(mBox);
        }
        else
        {
            msg *tmpMsg;
            // we need to put one mail in this mailbox
            if (mBox->nMaxMessages <= mBox->nMessages)
            {
                // cut the relation between the task and the message
                mBox->pHead->pNext->pBlock->pMessage = NULL;
                removeFirstMsg(mBox);
            }
            // the new message
            tmpMsg = (msg *)malloc(sizeof(msg));
            if (tmpMsg == NULL)
            {
                return FAIL;
            }

            tmpMsg->pData = malloc(mBox->nDataSize);
            if (tmpMsg->pData == NULL)
            {
                free(tmpMsg);
                return FAIL;
            }
            memcpy(tmpMsg->pData, Data, mBox->nDataSize);
            tmpMsg->Status = SENDER;
            tmpMsg->pBlock = NULL;

            appendMsg(mBox, tmpMsg, 1);
        }
        LoadContext();
    }
    return OK;
}

exception wait(uint nTicks)
{
    // disable interrupt
    isr_off();
    readylist->pHead->pNext->nTCnt = Ticks + nTicks;
    moveFirst(readylist, timerlist, -1);
    // update running pointer
    nextTask = readylist->pHead->pNext->pTask;
    SwitchContext();
    else
    {
        if (Running->DeadLine >= Ticks)
        {
            returnValue = DEADLINE_REACHED;
        }
        else
        {
            returnValue = OK;
        }
    }
    return returnValue;
}

void set_ticks(uint nTicks)
{
    Ticks = nTicks;
}

uint ticks(void)
{
    return Ticks;
}

uint deadline(void)
{
    return Running->DeadLine;
}

void TimerInt(void)
{
    listobj *iterator;
    Ticks++;

    iterator = timerlist->pHead->pNext;
    // for timer list
    while (iterator != timerlist->pTail)
    {
        if (iterator->nTCnt <= Ticks)
        {
            iterator = iterator->pNext;
            moveFirst(timerlist, readylist, 1);
            // update running pointer
            Running = readylist->pHead->pNext->pTask;
        }
        else
        {
            break;
        }
    }
    // for waiting list

    iterator = waitinglist->pHead->pNext;
    while (iterator != waitinglist->pTail)
    {
        if (iterator->pTask->DeadLine <= Ticks)
        {
            iterator = iterator->pNext;
            moveFirst(waitinglist, readylist, 1);

            // update running pointer
            Running = readylist->pHead->pNext->pTask;
        }
        else
        {
            break;
        }
    }
}

/*
-----------------------------------------------------------------
List functions: Lists used as the bulk of the project.
-----------------------------------------------------------------
*/
// makes new lists and checks for nulls used in initate
static list *newList(void)
{
    list *retList;
    listobj *pHead, *pTail;
    retList = (list *)malloc(1 * sizeof(list));
    if (retList == NULL)
    {
        return NULL;
    }
    pHead = (listobj *)malloc(1 * sizeof(listobj));
    if (pHead == NULL)
    {
        free(retList);
        return NULL;
    }
    pTail = (listobj *)malloc(1 * sizeof(listobj));
    if (pTail == NULL)
    {
        free(pHead);
        free(retList);
        return NULL;
    }
    retList->pHead = pHead;
    retList->pTail = pTail;

    retList->pHead->pNext = retList->pTail;

    return retList;
}

static listobj *newListObj(TCB *TCBObj)
{
    listobj *tmpListObj;
    tmpListObj = (listobj *)malloc(1 * sizeof(listobj));
    if (tmpListObj == NULL)
    {
        return NULL;
    }
    tmpListObj->pTask = TCBObj;
    tmpListObj->pMessage = NULL;
    return tmpListObj;
}

static int insertR(list *incList, TCB *TCBObj)
{
    listobj *tmpListObj = newListObj(TCBObj);
    if (tmpListObj == NULL)
    {
        return FAIL;
    }
    insert(incList, tmpListObj, 1);
    return OK;
}

static void removeFirst(list *incList)
{

    listobj *firstObj = incList->pHead->pNext;
    // unlink it from the list
    incList->pHead->pNext = firstObj->pNext;
    firstObj->pNext->pPrevious = incList->pHead;

    free(firstObj->pTask);
    free(firstObj);
}

static void removeList(list *incList)
{
    while (!isEmpty(incList))
    {
        removeFirst(incList);
    }
    free(incList->pHead);
    free(incList->pTail);
    free(incList);
}

static void moveFirst(list *listFrom, list *listTo, int sortedby)
{
    // get the first element
    listobj *first = listFrom->pHead->pNext;
    // unlink it from listFrom
    listFrom->pHead->pNext = first->pNext;
    first->pNext->pPrevious = first->pPrevious;

    // insert it into listTo
    insert(listTo, first, sortedby);
}

static void move(listobj *object, list *listFrom, list *listTo,
                 int sortedby)
{
    // unlink it from listFrom
    object->pPrevious->pNext = object->pNext;
    object->pNext->pPrevious = object->pPrevious;

    // insert it into listTo
    insert(listTo, object, sortedby);
}

static void insert(list *incList, listobj *element, int sortedby)
{

    if (isEmpty(incList))
    {
        incList->pHead->pNext = element;
        incList->pTail->pPrevious = element;
        element->pPrevious = incList->pHead;
        element->pNext = incList->pTail;
    }
    else
    {
        // there are some elements in this list, we need to find the
        // right position
        listobj *iterator = incList->pHead->pNext;
        while (iterator != incList->pTail)
        {
            //Checks if we need to sort by deadline or nTCnt
            if (sortedby == 1)
            {
                if ((element->pTask->DeadLine) < (iterator->pTask->DeadLine))
                {
                    break;
                }
            }
            else
            {
                if (element->nTCnt < iterator->nTCnt)
                {
                    break;
                }
            }
            iterator = iterator->pNext;
        }
        element->pNext = iterator;
        element->pPrevious = iterator->pPrevious;
        iterator->pPrevious->pNext = element;
        iterator->pPrevious = element;
    }
}
// checks if incoming list is empty
static int isEmpty(list *incList)
{
    if (incList->pHead->pNext == incList->pTail)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

static int getSize(list *temp)
{
    int x;
    listobj *iter;
    x = 0;
    iter = temp->pHead->pNext;
    while (iter != temp->pTail)
    {
        iter = iter->pNext;
        x++;
    }
    return x;
}
