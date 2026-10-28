import { KSM, KSMWebMessageTransport } from '../../../frontend/src/index';
import type { ContractAdapter } from '../helpers';

export class FrontendContractAdapter implements ContractAdapter {
  public constructor(webview: ConstructorParameters<typeof KSMWebMessageTransport>[0]) {
    KSM.initialize(new KSMWebMessageTransport(webview));
  }

  public invoke<TResult>(command: string, args: unknown): Promise<TResult> {
    return KSM.invoke<TResult, unknown>(command, args);
  }
}
