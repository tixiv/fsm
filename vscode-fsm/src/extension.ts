import * as vscode from "vscode";
import { Delayer } from "vscode-languageclient/$test/common/utils/async";
import {
    LanguageClient,
    LanguageClientOptions,
    SelectedCompletionInfo,
    ServerOptions
} from "vscode-languageclient/node";

let client: LanguageClient | undefined;

function createClient(): LanguageClient {
    const workspaceFolder = vscode.workspace.workspaceFolders?.[0];

    if (!workspaceFolder) {
        throw new Error("FSM language server requires an open workspace");
    }

    const server = workspaceFolder.uri.fsPath + "/bin/fsmd";

    const serverOptions: ServerOptions = {
        command: server,
        args: []
    };

    const clientOptions: LanguageClientOptions = {
        documentSelector: [
            {
                scheme: "file",
                language: "fsm"
            }
        ],
        //traceOutputChannel: vscode.window.createOutputChannel("FSM Language Server")
    };

    return new LanguageClient(
        "fsmd",
        "FSM Language Server",
        serverOptions,
        clientOptions
    );
}

export function activate(context: vscode.ExtensionContext) {
    context.subscriptions.push(
        vscode.commands.registerCommand(
            "fsm.startLanguageServer",
            async () => {
                await startLanguageServer();
            }
        )
    );

    context.subscriptions.push(
        vscode.commands.registerCommand(
            "fsm.stopLanguageServer",
            async () => {
                await stopLanguageServer();
            }
        )
    );

    context.subscriptions.push(
        vscode.commands.registerCommand(
            "fsm.restartLanguageServer",
            async () => {
                await restartLanguageServer();
            }
        )
    );

    startLanguageServer();
}

async function startLanguageServer() {
    if (client !== undefined) {
        return;
    }

    client = createClient();
    await client.start();
}

async function stopLanguageServer() {
    if (client === undefined) {
        return;
    }

    await client.stop();
    client = undefined;
}

async function restartLanguageServer() {
    await stopLanguageServer();
    await startLanguageServer();
}

export function deactivate(): Thenable<void> | undefined {
    return client?.stop();
}