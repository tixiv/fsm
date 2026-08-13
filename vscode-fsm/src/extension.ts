import * as vscode from "vscode";
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions
} from "vscode-languageclient/node";

let client: LanguageClient;



export function activate(context: vscode.ExtensionContext) {
    const serverOptions: ServerOptions = {
        command: "/home/tixiv/work/fsm/fsmd",
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

    client = new LanguageClient(
        "fsmd",
        "FSM Language Server",
        serverOptions,
        clientOptions
    );

    client.start();
}

export function deactivate(): Thenable<void> | undefined {
    return client?.stop();
}