import type {
  KSMError,
  KSMEvent,
  KSMRequest,
  KSMResponse,
  KSMTransport
} from './types';

const DEFAULT_TIMEOUT_MS = 10000;

interface PendingRequest<TResult> {
  resolve: (value: TResult) => void;
  reject: (reason: Error) => void;
  timer: ReturnType<typeof setTimeout>;
}

export class KSMBus {
  private readonly pending = new Map<string, PendingRequest<unknown>>();
  private readonly eventListeners = new Map<string, Set<(payload: unknown) => void>>();
  private readonly unsubscribeTransport: () => void;
  private nextRequestId = 0;
  private readonly subscribedEvents = new Set<string>();

  public constructor(private readonly transport: KSMTransport) {
    this.unsubscribeTransport = transport.onMessage((message) => {
      if (message.type === 'response') {
        this.resolveResponse(message);
        return;
      }
      this.dispatchEvent(message);
    });
  }

  public dispose(): void {
    this.unsubscribeTransport();
    for (const pending of this.pending.values()) {
      clearTimeout(pending.timer);
      pending.reject(new Error('Kasumi bridge was disposed.'));
    }
    this.pending.clear();
    this.eventListeners.clear();
    this.subscribedEvents.clear();
  }

  public invoke<TResult, TArgs>(
    cmd: string,
    args: TArgs,
    timeoutMs: number = DEFAULT_TIMEOUT_MS
  ): Promise<TResult> {
    if (!Number.isFinite(timeoutMs) || timeoutMs <= 0) {
      return Promise.reject(new Error('Kasumi timeoutMs must be positive.'));
    }
    const id = `req-${++this.nextRequestId}`;
    const request: KSMRequest<TArgs> = {
      type: 'request',
      id,
      cmd,
      args,
      timeoutMs
    };

    return new Promise<TResult>((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(id);
        this.transport.send({ type: 'cancel', id });
        reject(this.createError('TIMEOUT', `Kasumi request timed out: ${cmd}`));
      }, timeoutMs);

      this.pending.set(id, {
        resolve: resolve as (value: unknown) => void,
        reject,
        timer
      });
      this.transport.send(request as KSMRequest<unknown>);
    });
  }

  public cancel(id: string): void {
    const pending = this.pending.get(id);
    if (pending) {
      clearTimeout(pending.timer);
      this.pending.delete(id);
      pending.reject(
        this.createError('CANCELLED', `Kasumi request was cancelled: ${id}`)
      );
    }
    this.transport.send({ type: 'cancel', id });
  }

  public on<TPayload>(
    event: string,
    listener: (payload: TPayload) => void
  ): () => void {
    const listeners = this.eventListeners.get(event) ?? new Set();
    listeners.add(listener as (payload: unknown) => void);
    this.eventListeners.set(event, listeners);
    return (): void => {
      listeners.delete(listener as (payload: unknown) => void);
      if (listeners.size === 0) {
        this.eventListeners.delete(event);
      }
    };
  }

  public trackSubscription(event: string): void {
    this.subscribedEvents.add(event);
  }

  public untrackSubscription(event: string): void {
    this.subscribedEvents.delete(event);
  }

  public trackedSubscriptions(): string[] {
    return [...this.subscribedEvents];
  }

  private resolveResponse(response: KSMResponse<unknown>): void {
    const pending = this.pending.get(response.id);
    if (!pending) {
      return;
    }
    clearTimeout(pending.timer);
    this.pending.delete(response.id);
    if (response.ok) {
      pending.resolve(response.data);
      return;
    }
    pending.reject(this.toError(response.error));
  }

  private dispatchEvent(event: KSMEvent): void {
    const listeners = this.eventListeners.get(event.event);
    if (!listeners) {
      return;
    }
    for (const listener of listeners) {
      listener(event.payload);
    }
  }

  private toError(error: KSMError | undefined): Error {
    if (!error) {
      return new Error('Kasumi bridge returned an unknown error.');
    }
    const result = this.createError(error.code, error.message);
    if (error.detail !== undefined) {
      Object.assign(result, { detail: error.detail });
    }
    return result;
  }

  private createError(code: string, message: string): Error {
    const error = new Error(message);
    error.name = code;
    return error;
  }
}
