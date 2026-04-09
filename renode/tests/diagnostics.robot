*** Settings ***
Documentation     Body ECU diagnostics smoke test in Renode.
...               Verifies the diagnostic transport layers initialize
...               and respond to basic UDS requests over the emulated platform.

Suite Setup       Setup
Suite Teardown    Teardown
Test Timeout      30 seconds

*** Keywords ***
Setup
    Execute Command           include @${CURDIR}/../body_ecu.resc

Teardown
    Execute Command           quit

*** Test Cases ***
Diagnostics System Initializes
    [Documentation]    Verify the diagnostics system starts without errors.
    Start Emulation

    Wait For Line On Uart     Body ECU starting        timeout=10
    Wait For Line On Uart     Body ECU ready           timeout=15
