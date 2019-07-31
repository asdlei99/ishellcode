/*
WIN64��������ģ��
���ߣ�W1nds
*/


extern "C"
{
#include <ntifs.h>
#include <ntintsafe.h>
#include <ntddk.h>
#include <intrin.h>
}



#include "GLOBAL.h"
#include "WDKExt/Wdk.h"
extern "C" DRIVER_INITIALIZE DriverEntry;

#define	DEVICE_NAME			L"\\Device\\ishellcode"
#define LINK_NAME			L"\\DosDevices\\ishellcode"
#define LINK_GLOBAL_NAME	L"\\DosDevices\\Global\\ishellcode"



#define IOCTL_IO_TEST		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)



VOID DriverUnload(PDRIVER_OBJECT pDriverObj)
{
	UNICODE_STRING strLink;
	debug_msg("DriverUnload\n");
	
	//ɾ���������Ӻ��豸
	RtlInitUnicodeString(&strLink, LINK_NAME);
	IoDeleteSymbolicLink(&strLink);
	IoDeleteDevice(pDriverObj->DeviceObject);
}

NTSTATUS DispatchCreate(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
	debug_msg("DispatchCreate\n");
	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}


NTSTATUS DispatchClose(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
	debug_msg("DispatchClose\n");
	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}


NTSTATUS UnSupportedIrpFunction(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	NTSTATUS NtStatus = STATUS_NOT_SUPPORTED;
	debug_msg("Unsupported Irp Function \n");
	return NtStatus;
}

typedef struct tagKeyBUF
{
	DWORD_PTR uMyPid;
	DWORD_PTR uTargetPid;
	DWORD_PTR pShellAddr;
	DWORD_PTR uShellSize;
}KEYBUF, *PKEYBUF;

NTSTATUS MyDDKWrite(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrP)
{

	NTSTATUS status = STATUS_SUCCESS;

	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(pIrP);
	ULONG ulWriteLength = stack->Parameters.Write.Length;

	ULONG mdl_length = MmGetMdlByteCount(pIrP->MdlAddress);                    //��ȡ�������ĳ���  
	PVOID  mdl_address = MmGetMdlVirtualAddress(pIrP->MdlAddress);     //��ȡ�������������ַ  
	ULONG mdl_offset = MmGetMdlByteOffset(pIrP->MdlAddress);                   //���ػ�������ƫ��  

	debug_msg("mdl_length=%x ulWriteLength=%x\n", mdl_length, ulWriteLength);

	if (mdl_length != ulWriteLength)
	{
		//MDL�ĳ���Ӧ�úͶ�������ȣ�����ò���Ӧ����Ϊ���ɹ�  
		pIrP->IoStatus.Information = 0;
		status = STATUS_UNSUCCESSFUL;
	}
	else
	{
		//���Ǹ�MmGetSystemAddressForMdlSafe�õ����ں�ģʽ�µ�Ӱ��  
		PVOID kernel_address = MmGetSystemAddressForMdlSafe(pIrP->MdlAddress, NormalPagePriority);
		if (mdl_length==sizeof(KEYBUF))
		{
			HANDLE ProcessID = (HANDLE)((PKEYBUF)kernel_address)->uMyPid;
			PVOID pShell = (PVOID)((PKEYBUF)kernel_address)->pShellAddr;
			DWORD_PTR dwSize = ((PKEYBUF)kernel_address)->uShellSize;
			HANDLE TargetProcessID = (HANDLE)((PKEYBUF)kernel_address)->uTargetPid;
			PVOID pShellBuf = ExAllocatePoolWithTag(PagedPool, dwSize, MY_POOL_TAG);

			//������ȥ�����ڴ� ����ֱ���û��㴫�ݸ�shell�ļ���·���ں�ȥ��ȡ
			PEPROCESS hProcess;
			PsLookupProcessByProcessId(ProcessID, &hProcess);
			KAPC_STATE apc_state;
			KeStackAttachProcess(hProcess, &apc_state);
			if (MmIsAddressValid(pShell))
			{
				//ProbeForRead((CONST PVOID)(PVOID)pUserBuf, uMemLoadSize, sizeof(CHAR));
				RtlCopyMemory((PVOID)pShellBuf, pShell, dwSize);
			}
			else
			{
				debug_msg("2:%llx %llx\n", ProcessID, pShell);
			}
			KeUnstackDetachProcess(&apc_state);


			KernelInjectProcess(TargetProcessID,pShellBuf,dwSize);
			if (pShellBuf)
				ExFreePoolWithTag(pShellBuf, MY_POOL_TAG);
		}
		pIrP->IoStatus.Information = ulWriteLength;
	}


	//���IRP  
	pIrP->IoStatus.Status = status;                                                                    //�������״̬  
	IoCompleteRequest(pIrP, IO_NO_INCREMENT);                                        //���IRP  

	return status;
}


NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObj, PUNICODE_STRING pRegistryString)
{

	NTSTATUS status = STATUS_SUCCESS;
	UNICODE_STRING ustrLinkName;
	UNICODE_STRING ustrDevName;
	PDEVICE_OBJECT pDevObj;

	for (;;)
	{
		status = wdk::WdkInitSystem();
		if (!NT_SUCCESS(status))
		{
			debug_msg("WdkInitSystem failed\n");
			break;
		}
		break;
	}
	debug_msg("osver: %d \n", wdk::NtSystemVersion);
	if (!NT_SUCCESS(status))
	{
		DriverUnload(pDriverObj);
		return status;
	}

	//���÷ַ�������ж������
	for (UINT32 uiIndex = 0; uiIndex < IRP_MJ_MAXIMUM_FUNCTION; uiIndex++)
	{
		pDriverObj->MajorFunction[uiIndex] = UnSupportedIrpFunction;
	}
	pDriverObj->MajorFunction[IRP_MJ_WRITE] = MyDDKWrite;  
	pDriverObj->DriverUnload = DriverUnload;

	RtlInitUnicodeString(&ustrDevName, DEVICE_NAME);
	status = IoCreateDevice(pDriverObj, 0, &ustrDevName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &pDevObj);
	if (!NT_SUCCESS(status))
		return status;
	//pDevObj->Flags |= DO_BUFFERED_IO;
	pDevObj->Flags |= DO_DIRECT_IO;
	pDevObj->Flags &= (~DO_DEVICE_INITIALIZING);
	
	RtlInitUnicodeString(&ustrLinkName, LINK_NAME);

	status = IoCreateSymbolicLink(&ustrLinkName, &ustrDevName);
	if (!NT_SUCCESS(status))
	{
		IoDeleteDevice(pDevObj);
		return status;
	}
	debug_msg("DriverEntry\n");

	return STATUS_SUCCESS;
}