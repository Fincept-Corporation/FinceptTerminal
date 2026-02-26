// Redux store configuration for Kepler.gl
import { createStore, combineReducers, applyMiddleware, compose } from 'redux';
// @ts-expect-error no types for react-palm
import { taskMiddleware } from 'react-palm/tasks';
// @ts-expect-error no types for kepler.gl
import keplerGlReducer from 'kepler.gl/reducers';

const customizedKeplerGlReducer = keplerGlReducer.initialState({
  uiState: {
    readOnly: false,
    currentModal: null
  }
});

const reducers = combineReducers({
  keplerGl: customizedKeplerGlReducer,
  // Add other reducers if needed
});

const middlewares = [taskMiddleware];

// Enable Redux DevTools in development
const composeEnhancers =
  (typeof window !== 'undefined' && (window as any).__REDUX_DEVTOOLS_EXTENSION_COMPOSE__) || compose;

export const keplerStore = createStore(
  reducers,
  {},
  composeEnhancers(applyMiddleware(...middlewares))
);

export type KeplerRootState = ReturnType<typeof reducers>;
